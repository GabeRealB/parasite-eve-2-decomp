#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/collision.h"
#include "collision.h"
#include "gameplay/hud_sprites.h"
#include "item_use.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"

#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task_types.h"

/// Q24 facing cutoff: at least 3/4 alignment opposite the tested normal.
enum { WORLD_COLLISION_TRIGGER_FACING_DOT_MAX = -(ONE * ONE * 3 / 4) };

/// Inclusive squared-distance cutoff for the strict 500-game-unit near test.
enum { WORLD_COLLISION_TRIGGER_NEAR_DISTANCE_SQUARED_MAX = 500 * 500 - 1 };

/// Temporary endpoint transforms for a view-space collision-grid segment query.
///
/// This scratch-stack block remains live through the face-candidate scan.
/// Coordinates use game units. Grid X/Z coordinates are initially truncated to
/// signed 16 bits, then the scan may extend them in place; Y is zero. The SDK
/// vectors' fourth components are unused and left uninitialized.
typedef struct {
    VECTOR viewEndpoint;     // View-space endpoint promoted from signed 16-bit coordinates
    VECTOR rotatedEndpoint;  // Endpoint after inverse view rotation, before translation and grid bias
    VECTOR gridEndpoints[2]; // Biased-grid XZ endpoints, in input order; extended by the candidate scan
} _WorldCollisionGridQueryScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionGridQueryScratch, 0x40);

/// Temporary XZ geometry for selecting collision-grid face candidates.
///
/// Positions and displacements use game units relative to the biased grid
/// origin. Only X and Z of `cellCenter` and `displacement` are initialized or
/// read. The block lives on the scratch stack for one grid scan.
typedef struct {
    VECTOR  segmentDelta;     // First endpoint minus second, with Y zero
    SVECTOR segmentDirection; // Direction from second endpoint toward first; 4096 per unit
    SVECTOR cellCenter;       // Biased-grid XZ cell centre, truncated to signed 16-bit coordinates
    SVECTOR displacement;     // XZ endpoint extension or cell/query offset, truncated to signed 16 bits
} _WorldCollisionGridCandidateScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionGridCandidateScratch, 0x28);

/// Temporary view-space segment and intersection storage for collision-grid probes.
///
/// Endpoint and hit coordinates use game units; signed 16-bit inputs and hits
/// are promoted into the endpoint array. Each accepted hit replaces endpoint 0
/// while endpoint 1 and the original normalized direction remain fixed.
/// The face test reads both arrays and may write a candidate intersection even
/// when it later rejects the face; consume that point only on acceptance.
/// This entire scratch-stack block stays live through the nested candidate and
/// face tests and is released before the probe returns. SDK fourth components
/// have no role in the query.
typedef struct {
    VECTOR  endpoints[2]; // View-space segment: [0] clipped target, [1] fixed start
    VECTOR  delta;        // Normalization workspace, initialized to original target minus start
    SVECTOR ray[2];       // [0] Start-to-target direction, 4096 per unit; [1] candidate intersection
} _WorldCollisionGridProbeScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionGridProbeScratch, 0x40);

/// Temporary vectors for placing a capsule body's segment and finding its axis.
///
/// Each local endpoint is the capsule's end offset plus the body's position,
/// truncated to signed 16 bits, in game units. The body's cached transform
/// rotates it; the caller's endpoint is that result plus the translation.
/// The same vector then holds endpoint 0 minus endpoint 1 for normalization.
/// The block lives on the scratch stack for one segment calculation. The SDK
/// vectors' fourth components are unused and left uninitialized.
typedef struct {
    union {
        VECTOR rotatedEndpoint; // Local endpoint after the body's rotation, before its translation
        VECTOR segmentDelta;    // Placed endpoint 0 minus endpoint 1; the axis before normalization
    } work;
    SVECTOR localEndpoint;      // Capsule end offset plus the body's position, in the body's frame
} _WorldCollisionCapsuleSegmentScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionCapsuleSegmentScratch, 0x18);

/// Query-space geometry for testing a motion sphere against a trigger quad.
///
/// The scratch stack reserves this block for one test and releases it on
/// every exit. The trigger's cached transform maps its origin, corner offsets
/// and normal into the query space that holds the placed sphere centre.
/// Positions use game units. The face normal uses 4096 per unit. The
/// broad-phase distance check comes first; corner 0 and the normal are placed
/// for the plane test, and the other corners only once the sphere reaches the
/// plane from its negative side. `leveledOrigin` and
/// `work.leveledOriginToSphere` serve the near-or-facing kind's facing gate
/// alone. Each SDK vector's fourth component is unused and left uninitialized.
typedef struct {
    VECTOR corners[4];                // Query-space corners in strip order, game units
    VECTOR origin;                    // Query-space trigger origin, which places the corners
    VECTOR faceNormal;                // Query-space plane normal, 4096 per unit
    union {
        VECTOR sphereToOrigin;        // Trigger origin minus sphere centre, for the broad-phase distance
        VECTOR leveledOriginToSphere; // Rotated leveled origin, then sphere centre minus its placed point, normalized in place
        VECTOR edgeDisplacement;      // End corner minus start corner of the current edge
    } work;
    VECTOR  edgePlaneNormal;          // Separating normal of the current edge; length follows the edge
    VECTOR  sphereCenter;             // Body's local sphere centre placed in query space, game units
    SVECTOR leveledOrigin;            // Trigger-local origin with Y replaced by the body's height, truncated to signed 16 bits
} _WorldCollisionTriggerSphereScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionTriggerSphereScratch, 0x98);

/// Query-space geometry for testing one segment against an occluder quad.
///
/// The scratch stack reserves this block for the test and releases it on
/// every exit. The current view transform maps the occluder's room-space
/// origin, corner offsets and normal into the segment's query space.
/// Positions use game units. The face normal uses 4096 per unit. Corner 0
/// is placed before the plane test; the other corners are placed only after
/// the segment meets the plane. `position` holds the transformed origin
/// until then, and the plane intersection afterwards. Each vector's fourth
/// component is unused and left uninitialized.
typedef struct {
    VECTOR corners[4];       // Query-space corners in strip order, game units
    union {
        VECTOR origin;       // Transformed quad origin, used to place the corners
        VECTOR intersection; // Segment-plane hit, written after the corners are placed
    } position;
    VECTOR faceNormal;       // Query-space plane normal, 4096 per unit
    VECTOR edgeDisplacement; // End corner minus start corner, game units
    VECTOR edgePlaneNormal;  // Separating normal of the current edge; length follows the edge
} _WorldCollisionOccluderSegmentScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionOccluderSegmentScratch, 0x80);

/// Temporary endpoint transforms for a collision body's grid-candidate query.
///
/// The local endpoints span the body's footprint in its own frame: a motion
/// sphere's centre displaced one radius forward and back along its motion
/// direction, or a capsule's two end offsets plus the body's position. Only X
/// and Z are kept, truncated to signed 16 bits. `bodyToRoom` removes the view
/// transform composed into the body's cached matrix, so rotating an endpoint,
/// then adding the X/Z translation and the grid bias, gives biased-grid
/// coordinates. The candidate scan may extend the grid endpoints in place.
/// Coordinates use game units. The block lives on the scratch stack through
/// that scan. The SDK vectors' fourth components are unused and left
/// uninitialized.
typedef struct {
    VECTOR  gridEndpoints[2];  // Biased-grid XZ endpoints, in local-endpoint order, with Y zero
    SVECTOR localEndpoints[2]; // Body-frame XZ endpoints, with Y zero
    MATRIX  bodyToRoom;        // Body frame to room space: the body's cached matrix relative to the view's
} _WorldCollisionGridBodyQueryScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionGridBodyQueryScratch, 0x50);

/// Temporary view-space segment and hit storage for a motion sphere's floor query.
///
/// The segment is the body's local Y axis, one radius either side of its
/// position, placed by the body's cached transform. Endpoint 0 is the end
/// at position Y plus the radius; the direction runs from endpoint 1 toward
/// it. Each accepted floor hit replaces endpoint 0, so a later floor has to
/// cross the shortened segment, while endpoint 1 and the direction stay
/// fixed. `placedEndpoint` keeps endpoint 0 as first placed, and each hit's
/// contact distance is measured from it. The face test may write a candidate
/// intersection even when it later rejects the face; consume that point only
/// on acceptance. Positions use game units. The block lives on the scratch
/// stack through the nested candidate scan, segment placement and face
/// tests. The SDK vectors' fourth components are unused and left
/// uninitialized.
typedef struct {
    VECTOR  endpoints[2];   // View-space segment: [0] end clipped to the latest accepted hit, [1] fixed far end
    VECTOR  placedEndpoint; // Endpoint 0 before any hit replaced it
    VECTOR  hitOffset;      // Placed endpoint 0 minus the accepted hit; its length is the contact distance
    SVECTOR ray[2];         // [0] Endpoint 1 to endpoint 0 direction, 4096 per unit; [1] candidate intersection
} _WorldCollisionFloorQueryScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionFloorQueryScratch, 0x50);

/// Temporary view-space segment and hit storage for a capsule body's grid-contact scan.
///
/// The segment joins the capsule's two ends, placed by the body's cached
/// transform; the direction runs from endpoint 1 toward endpoint 0. A face is
/// crossed only from its front, with endpoint 1 in front of it and endpoint 0
/// behind. A body that clips to its grid contact starts endpoint 0 at the
/// point of the grid contact it already holds, when it has one, and each
/// accepted hit then replaces endpoint 0, so a later face has to cross the
/// shortened segment; endpoint 1 and the direction stay fixed. Other bodies
/// record every crossed face against the unshortened segment. The face test
/// may write a candidate intersection even when it later rejects the face;
/// consume that point only on acceptance. Positions use game units. The block
/// lives on the scratch stack through the nested candidate scan, segment
/// placement and face tests. The SDK vectors' fourth components are unused
/// and left uninitialized.
typedef struct {
    VECTOR  endpoints[2]; // View-space segment: [0] end a clipping body shortens to its latest accepted hit, [1] fixed far end
    SVECTOR ray[2];       // [0] Endpoint 1 to endpoint 0 direction, 4096 per unit; [1] candidate intersection
} _WorldCollisionCapsuleGridContactScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionCapsuleGridContactScratch, 0x30);

/// Query-space geometry for testing a motion sphere against a view boundary.
///
/// The scratch stack reserves this block for one test and releases it on
/// every exit. The movement direction is found first, in the space of the
/// body's coordinate parent, and gates everything else: the boundary is
/// tested only when the body moved against its untransformed normal. The
/// trigger's cached transform then maps its origin, corner offsets and normal
/// into the query space that holds the placed sphere centre. Positions use
/// game units; the direction and the face normal use 4096 per unit. The
/// broad-phase distance check comes next; corner 0 and the normal are placed
/// for the plane test, and the other corners only once the sphere reaches the
/// plane from its negative side. The geometry fields keep the offsets they
/// have in `_WorldCollisionTriggerSphereScratch`. Each SDK vector's fourth
/// component is unused and left uninitialized.
typedef struct {
    VECTOR corners[4];           // Query-space corners in strip order, game units
    VECTOR origin;               // Query-space boundary origin, which places the corners
    VECTOR faceNormal;           // Query-space plane normal, 4096 per unit
    union {
        VECTOR sphereToOrigin;   // Boundary origin minus sphere centre, for the broad-phase distance
        VECTOR edgeDisplacement; // End corner minus start corner of the current edge
    } work;
    VECTOR edgePlaneNormal;      // Separating normal of the current edge; length follows the edge
    VECTOR sphereCenter;         // Body's local sphere centre placed in query space, game units
    u8     unk90[0x10];          // Reserved with the block but never read or written; role unproven
    VECTOR movementDirection;    // Body's coordinate translation minus the position it moved from, normalized in place
} _WorldCollisionViewBoundarySphereScratch;
STATIC_ASSERT_SIZEOF(_WorldCollisionViewBoundarySphereScratch, 0xB0);

/* Define BSS before API headers to preserve first-declaration order. */
s32 Gp_RoomParams[8];

WorldCollisionGrid* Gp_GridParams;

u8 D_80115450[256];

WorldCollisionOccluder* D_80115550;

WorldCollisionTrigger* Gp_Obj4CList;

#include "gameplay/world_collision.h"

#include "world_collision.h"

static __inline__ void Gp_ObjWorldPosInline(WorldCollisionBody* obj, VECTOR* pos);

static void func_800DDC2C(WorldCollisionBody* arg0);

static void func_800DE150(WorldCollisionBody* arg0);

static void func_800DE2C0(VECTOR* arg0, s32 arg1);

static void func_800DEAFC(SVECTOR* arg0, SVECTOR* arg1);

static __inline__ void Gp_ObjWorldPosInline(WorldCollisionBody* obj, VECTOR* pos)
{
    u8*     h;
    VECTOR* vec;
    h                          = SCRATCH_STACK_CURSOR(u8);
    vec                        = (VECTOR*)(h - 0x30);
    SCRATCH_STACK_CURSOR(void) = vec;
    gte_SetRotMatrix(&obj->coord->workm);
    gte_ldv0(&obj->pos);
    gte_rtv0();
    gte_stlvnl(vec);
    pos->vx = (obj->coord)->workm.t[0] + ((VECTOR*)(h - 0x30))->vx;
    pos->vy = (obj->coord)->workm.t[1] + vec->vy;
    pos->vz = (obj->coord)->workm.t[2] + vec->vz;
    SCRATCH_STACK_RELEASE_BYTES(0x30);
}

void func_800DD940(WorldCollisionBody* arg0)
{
    enum { WORLD_COLLISION_FLOOR_SURFACE_CLASS_MASK = 0xF };
    _WorldCollisionFloorQueryScratch* scratch;
    WorldCollisionContact*            slot;
    s32                               i;
    u16                               flags;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionFloorQueryScratch);
    for (i = 0; i < Gp_GridParams->faceCount; i++) {
        D_80115450[i] = 0;
    }
    func_800DDC2C(arg0);
    worldCollisionPlaceFloorSegment(arg0, scratch->endpoints, scratch->ray);
    scratch->placedEndpoint.vx = scratch->endpoints[0].vx;
    scratch->placedEndpoint.vy = scratch->endpoints[0].vy;
    scratch->placedEndpoint.vz = scratch->endpoints[0].vz;
    for (i = 0; i < Gp_GridParams->faceCount; i++) {
        if (D_80115450[i] &&
            Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex].vy < -0xDDA &&
            worldCollisionIntersectGridFace(i, scratch->endpoints, scratch->ray, arg0)) {
            slot  = arg0->context.motion->contacts;
            flags = slot->flags;
            if (flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                // Overlapping floors retain the higher class using unsigned comparison.
                if ((u32)(slot->key.value & WORLD_COLLISION_FLOOR_SURFACE_CLASS_MASK) < (u32)Gp_GridParams->faces[i].surfaceClass) {
                    slot->key.value = Gp_GridParams->faces[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID_FLOOR;
                }
            } else {
                slot->flags     = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                slot->key.value = Gp_GridParams->faces[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID_FLOOR;
            }
            slot->point              = scratch->ray[1];
            slot->response.direction = Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex];
            scratch->hitOffset.vx    = scratch->placedEndpoint.vx - scratch->ray[1].vx;
            scratch->hitOffset.vy    = scratch->placedEndpoint.vy - scratch->ray[1].vy;
            scratch->hitOffset.vz    = scratch->placedEndpoint.vz - scratch->ray[1].vz;
            slot->distance           = SquareRoot0(scratch->hitOffset.vx * scratch->hitOffset.vx +
                                                   scratch->hitOffset.vy * scratch->hitOffset.vy + scratch->hitOffset.vz * scratch->hitOffset.vz);
            scratch->endpoints[0].vx = scratch->ray[1].vx;
            scratch->endpoints[0].vy = scratch->ray[1].vy;
            scratch->endpoints[0].vz = scratch->ray[1].vz;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionFloorQueryScratch);
}

static void func_800DDC2C(WorldCollisionBody* arg0)
{
    s32                                  i;
    _WorldCollisionGridBodyQueryScratch* scratch;
    SVECTOR*                             motionDirection;
    MATRIX*                              bodyToRoom;

    motionDirection               = &arg0->context.motion->motionDirection;
    scratch                       = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionGridBodyQueryScratch);
    bodyToRoom                    = &scratch->bodyToRoom;
    scratch->localEndpoints[0].vx = (u16)arg0->pos.vx + ((motionDirection->vx * arg0->radius) >> 12);
    scratch->localEndpoints[0].vy = 0;
    scratch->localEndpoints[0].vz = (u16)arg0->pos.vz + ((motionDirection->vz * arg0->radius) >> 12);
    scratch->localEndpoints[1].vx = (u16)arg0->pos.vx + (-(motionDirection->vx * arg0->radius) >> 12);
    scratch->localEndpoints[1].vy = 0;
    scratch->localEndpoints[1].vz = (u16)arg0->pos.vz + (-(motionDirection->vz * arg0->radius) >> 12);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &arg0->coord->workm, bodyToRoom);
    gte_SetRotMatrix(bodyToRoom);
    for (i = 0; i < 2; i++) {
        gte_ldv0(&scratch->localEndpoints[i]);
        gte_rtv0();
        gte_stlvnl(&scratch->gridEndpoints[i]);
        scratch->gridEndpoints[i].vx = scratch->gridEndpoints[i].vx + scratch->bodyToRoom.t[0] + Gp_GridParams->xBias;
        scratch->gridEndpoints[i].vy = 0;
        scratch->gridEndpoints[i].vz = scratch->gridEndpoints[i].vz + scratch->bodyToRoom.t[2] + Gp_GridParams->zBias;
    }
    func_800DE2C0(scratch->gridEndpoints, 0);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridBodyQueryScratch);
}

void func_800DDDF8(WorldCollisionBody* obj)
{
    _WorldCollisionCapsuleGridContactScratch* scratch;
    WorldCollisionContact*                    slot;
    u16                                       flags;
    s32                                       i;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionCapsuleGridContactScratch);
    for (i = 0; i < Gp_GridParams->faceCount; i++) {
        D_80115450[i] = 0;
    }

    func_800DE150(obj);
    func_800DEC80(obj, scratch->endpoints, scratch->ray, 1);

    for (i = 0; i < Gp_GridParams->faceCount; i++) {
        if (D_80115450[i] != 0 && worldCollisionIntersectGridFace(i, scratch->endpoints, scratch->ray, obj) != 0) {
            slot = obj->context.capsule->contacts;
            if (obj->flags & WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT) {
                if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1]
                                      [Gp_GridParams->faces[i].surfaceClass]
                                          ->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
                    slot->distance           = 0;
                    slot->flags             |= WORLD_COLLISION_CONTACT_OCCUPIED;
                    slot->key.value          = Gp_GridParams->faces[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                    slot->point              = scratch->ray[1];
                    slot->response.direction = Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex];
                    scratch->endpoints[0].vx = scratch->ray[1].vx;
                    scratch->endpoints[0].vy = scratch->ray[1].vy;
                    scratch->endpoints[0].vz = scratch->ray[1].vz;
                }
            } else {
                for (;;) {
                    flags = slot->flags;
                    if (!(flags & WORLD_COLLISION_CONTACT_OCCUPIED)) {
                        slot->flags              = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                        slot->distance           = 0;
                        slot->key.value          = Gp_GridParams->faces[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                        slot->point              = scratch->ray[1];
                        slot->response.direction = Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex];
                        if (slot->flags & WORLD_COLLISION_CONTACT_LAST) {
                            void** head = SCRATCH_HEAD_ADDR;

                            SCRATCH_POP_BYTES_AT(head, sizeof(_WorldCollisionCapsuleGridContactScratch));
                            return;
                        }
                        break;
                    }
                    if (flags == (WORLD_COLLISION_CONTACT_OCCUPIED | WORLD_COLLISION_CONTACT_LAST)) {
                        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleGridContactScratch);
                        return;
                    }
                    slot++;
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleGridContactScratch);
}

static void func_800DE150(WorldCollisionBody* arg0)
{
    s32                                  i;
    _WorldCollisionGridBodyQueryScratch* scratch;
    SVECTOR*                             src;
    GfxCoord*                            coord;
    MATRIX*                              bodyToRoom;

    coord      = arg0->coord;
    scratch    = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionGridBodyQueryScratch);
    bodyToRoom = &scratch->bodyToRoom;
    src        = arg0->context.capsule->ends;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, bodyToRoom);
    gte_SetRotMatrix(bodyToRoom);
    for (i = 0; i < 2; i++) {
        scratch->localEndpoints[i].vx = (u16)src[i].vx + (u16)arg0->pos.vx;
        scratch->localEndpoints[i].vy = 0;
        scratch->localEndpoints[i].vz = (u16)src[i].vz + (u16)arg0->pos.vz;
        gte_ldv0(&scratch->localEndpoints[i]);
        gte_rtv0();
        gte_stlvnl(&scratch->gridEndpoints[i]);
        scratch->gridEndpoints[i].vx = scratch->gridEndpoints[i].vx + scratch->bodyToRoom.t[0] + Gp_GridParams->xBias;
        scratch->gridEndpoints[i].vy = 0;
        scratch->gridEndpoints[i].vz = scratch->gridEndpoints[i].vz + scratch->bodyToRoom.t[2] + Gp_GridParams->zBias;
    }
    func_800DE2C0(scratch->gridEndpoints, 1);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridBodyQueryScratch);
}

static void func_800DE2C0(VECTOR* arg0, s32 arg1)
{
    enum {
        WORLD_COLLISION_GRID_DIAGONAL_SCALE_Q7       = 0xB5,
        WORLD_COLLISION_GRID_DIAGONAL_FRACTION_BITS  = 7,
        WORLD_COLLISION_GRID_DIRECTION_FRACTION_BITS = 12
    };
    u8*                                  scratchCursor;
    _WorldCollisionGridCandidateScratch* scratch;
    WorldCollisionGrid*                  pointGrid;
    WorldCollisionGrid*                  segmentGrid;
    s32                                  thresh2;
    u32                                  cellSize;
    s32                                  half;
    s32                                  range;
    s32                                  thresh;
    s32                                  i;
    s32                                  j;
    s32                                  dot;
    s32                                  proj;
    s32                                  vz0;
    s32                                  vz1;
    s16*                                 ids;
    s16                                  id;

    scratchCursor            = SCRATCH_STACK_CURSOR(u8);
    cellSize                 = Gp_GridParams->cellSize;
    scratch                  = (_WorldCollisionGridCandidateScratch*)(SCRATCH_STACK_CURSOR(void) = scratchCursor - sizeof(*scratch));
    scratch->segmentDelta.vx = arg0[0].vx - arg0[1].vx;
    scratch->segmentDelta.vy = 0;
    vz0                      = arg0[0].vz;
    vz1                      = arg0[1].vz;
    scratch->segmentDelta.vz = vz0 - vz1;
    half                     = cellSize >> 1;
    // Expand the query footprint by the cell's approximate half diagonal.
    range = ((half * WORLD_COLLISION_GRID_DIAGONAL_SCALE_Q7) >> WORLD_COLLISION_GRID_DIAGONAL_FRACTION_BITS) + 1;
    VectorNormalS(&scratch->segmentDelta, &scratch->segmentDirection);

    if ((scratch->segmentDirection.vx == 0) && (scratch->segmentDirection.vz == 0)) {
        for (i = 0; i < Gp_GridParams->cellCountX; i++) {
            thresh = range * range;
            for (j = 0; j < Gp_GridParams->cellCountZ; j++) {
                pointGrid                = Gp_GridParams;
                scratch->cellCenter.vx   = i * pointGrid->cellSize + (pointGrid->cellSize >> 1);
                scratch->cellCenter.vz   = j * pointGrid->cellSize + (pointGrid->cellSize >> 1);
                scratch->displacement.vx = (u16)scratch->cellCenter.vx - (u16)arg0[0].vx;
                scratch->displacement.vz = (u16)scratch->cellCenter.vz - (u16)arg0[0].vz;
                if ((scratch->displacement.vx * scratch->displacement.vx) +
                        (scratch->displacement.vz * scratch->displacement.vz) <
                    thresh) {
                    ids = pointGrid->cellFaceIds[i * pointGrid->cellCountZ + j];
                    if (ids != NULL) {
                        // Cell lists select face candidates; their indices are signed.
                        while (*ids != WORLD_COLLISION_GRID_CELL_END) {
                            id             = *ids;
                            D_80115450[id] = 1;
                            ids++;
                        }
                    }
                }
            }
        }
    } else {
        // Extend both endpoints before reusing the displacement for cell-distance tests.
        scratch->displacement.vx = (scratch->segmentDirection.vx * range) >> WORLD_COLLISION_GRID_DIRECTION_FRACTION_BITS;
        scratch->displacement.vz = (scratch->segmentDirection.vz * range) >> WORLD_COLLISION_GRID_DIRECTION_FRACTION_BITS;
        arg0[0].vx              += scratch->displacement.vx;
        arg0[0].vz              += scratch->displacement.vz;
        arg0[1].vx              -= scratch->displacement.vx;
        arg0[1].vz              -= scratch->displacement.vz;
        for (i = 0; i < Gp_GridParams->cellCountX; i++) {
            thresh2 = range * range;
            for (j = 0; j < Gp_GridParams->cellCountZ; j++) {
                segmentGrid            = Gp_GridParams;
                scratch->cellCenter.vx = i * segmentGrid->cellSize + (segmentGrid->cellSize >> 1);
                scratch->cellCenter.vz = j * segmentGrid->cellSize + (segmentGrid->cellSize >> 1);
                dot                    = ((scratch->cellCenter.vx - arg0[0].vx) * scratch->segmentDirection.vx) +
                      ((scratch->cellCenter.vz - arg0[0].vz) * scratch->segmentDirection.vz);
                if (dot <= 0) {
                    proj = (((scratch->cellCenter.vx - arg0[1].vx) * scratch->segmentDirection.vx) +
                            ((scratch->cellCenter.vz - arg0[1].vz) * scratch->segmentDirection.vz)) >>
                           WORLD_COLLISION_GRID_DIRECTION_FRACTION_BITS;
                    if (proj > 0) {
                        scratch->displacement.vx = ((u16)arg0[1].vx + ((scratch->segmentDirection.vx * proj) >> WORLD_COLLISION_GRID_DIRECTION_FRACTION_BITS)) -
                                                   (u16)scratch->cellCenter.vx;
                        scratch->displacement.vz = ((u16)arg0[1].vz + ((scratch->segmentDirection.vz * proj) >> WORLD_COLLISION_GRID_DIRECTION_FRACTION_BITS)) -
                                                   (u16)scratch->cellCenter.vz;
                        if ((scratch->displacement.vx * scratch->displacement.vx) +
                                (scratch->displacement.vz * scratch->displacement.vz) <
                            thresh2) {
                            ids = segmentGrid->cellFaceIds[i * segmentGrid->cellCountZ + j];
                            if (ids != NULL) {
                                while (*ids != WORLD_COLLISION_GRID_CELL_END) {
                                    id             = *ids++;
                                    D_80115450[id] = 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridCandidateScratch);
}

s32 func_800DE7CC(SVECTOR* arg0, SVECTOR* arg1, SVECTOR* arg2, SVECTOR* arg3)
{
    WorldCollisionGrid*              grid;
    s32                              ret;
    _WorldCollisionGridProbeScratch* scratch;
    s32                              i;

    grid = Gp_GridParams;
    ret  = 0;
    if (grid == NULL) {
        return ret;
    }

    i       = 0;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionGridProbeScratch);
    if (ret < grid->faceCount) {
        do {
            D_80115450[i] = 0;
            i++;
        } while (i < Gp_GridParams->faceCount);
    }
    func_800DEAFC(arg0, arg1);
    // Keep the direction fixed as accepted intersections shorten the segment.
    scratch->endpoints[0].vx = arg0->vx;
    scratch->endpoints[0].vy = arg0->vy;
    scratch->endpoints[0].vz = arg0->vz;
    scratch->endpoints[1].vx = arg1->vx;
    scratch->endpoints[1].vy = arg1->vy;
    scratch->endpoints[1].vz = arg1->vz;
    scratch->delta.vx        = scratch->endpoints[0].vx - scratch->endpoints[1].vx;
    scratch->delta.vy        = scratch->endpoints[0].vy - scratch->endpoints[1].vy;
    scratch->delta.vz        = scratch->endpoints[0].vz - scratch->endpoints[1].vz;
    VectorNormalS(&scratch->delta, &scratch->ray[0]);
    for (i = 0; i < Gp_GridParams->faceCount; i++) {
        if (D_80115450[i] == 0) {
            continue;
        }
        if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1]
                              [Gp_GridParams->faces[i].surfaceClass]
                                  ->probePassThrough != WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
            continue;
        }
        if (worldCollisionIntersectGridFace(i, scratch->endpoints, scratch->ray, NULL) == 0) {
            continue;
        }
        if (arg2 != NULL) {
            arg2->vx = scratch->ray[1].vx;
            arg2->vy = scratch->ray[1].vy;
            arg2->vz = scratch->ray[1].vz;
        }
        if (arg3 != NULL) {
            arg3->vx = Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex].vx;
            arg3->vy = Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex].vy;
            arg3->vz = Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex].vz;
        }
        scratch->endpoints[0].vx = scratch->ray[1].vx;
        scratch->endpoints[0].vy = scratch->ray[1].vy;
        scratch->endpoints[0].vz = scratch->ray[1].vz;
        ret                      = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridProbeScratch);
    return ret;
}

static void func_800DEAFC(SVECTOR* arg0, SVECTOR* arg1)
{
    _WorldCollisionGridQueryScratch* scratchEnd;
    _WorldCollisionGridQueryScratch* scratch;
    VECTOR*                          rotatedEndpoint;

    scratchEnd = SCRATCH_STACK_CURSOR(_WorldCollisionGridQueryScratch);
    scratch    = scratchEnd - 1;
    // Convert each view endpoint to biased-grid XZ coordinates before scanning.
    scratch->viewEndpoint.vx                              = arg0->vx;
    scratch->viewEndpoint.vy                              = arg0->vy;
    scratch->viewEndpoint.vz                              = arg0->vz;
    rotatedEndpoint                                       = &scratch->rotatedEndpoint;
    SCRATCH_STACK_CURSOR(_WorldCollisionGridQueryScratch) = scratch;
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->viewEndpoint, rotatedEndpoint);
    {
        WorldCollisionGrid* grid = Gp_GridParams;

        scratch->gridEndpoints[0].vx = (s16)(scratch->rotatedEndpoint.vx + grid->xBias - grid->viewCoord->coord.t[0]);
        scratch->gridEndpoints[0].vy = 0;
        scratch->gridEndpoints[0].vz = (s16)(scratch->rotatedEndpoint.vz + grid->zBias - grid->viewCoord->coord.t[2]);
    }
    scratch->viewEndpoint.vx = arg1->vx;
    scratch->viewEndpoint.vy = arg1->vy;
    scratch->viewEndpoint.vz = arg1->vz;
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->viewEndpoint, rotatedEndpoint);
    {
        WorldCollisionGrid* grid = Gp_GridParams;

        scratch->gridEndpoints[1].vx = (s16)(scratch->rotatedEndpoint.vx + grid->xBias - grid->viewCoord->coord.t[0]);
        scratch->gridEndpoints[1].vy = 0;
        scratch->gridEndpoints[1].vz = (s16)(scratch->rotatedEndpoint.vz + grid->zBias - grid->viewCoord->coord.t[2]);
    }
    func_800DE2C0(scratch->gridEndpoints, 0);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridQueryScratch);
}

void func_800DEC80(WorldCollisionBody* arg0, VECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    _WorldCollisionCapsuleSegmentScratch* scratch;
    WorldCollisionCapsule*                rec;
    SVECTOR*                              src;
    WorldCollisionContact*                slot;
    s32                                   flags;
    s32                                   i;

    rec     = arg0->context.capsule;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionCapsuleSegmentScratch);
    i       = 0;

    if (arg3 == 0) {
        if (arg0->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {
            slot = arg0->context.capsule->contacts;
            for (;;) {
                flags = slot->flags;
                if (flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                    arg1->vx = slot->point.vx;
                    arg1->vy = slot->point.vy;
                    arg1->vz = slot->point.vz;
                    i        = 1;
                    goto done_search;
                }
                if (flags & WORLD_COLLISION_CONTACT_LAST) {
                    goto done_search;
                }
                slot++;
            }
        } else if (arg0->flags & WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT) {
            slot = arg0->context.capsule->contacts;
            for (;;) {
                if (slot->flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                    if ((slot->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_GRID) {
                        arg1->vx = slot->point.vx;
                        arg1->vy = slot->point.vy;
                        arg1->vz = slot->point.vz;
                        i        = 1;
                        goto done_search;
                    }
                }
                if (slot->flags & WORLD_COLLISION_CONTACT_LAST) {
                    goto done_search;
                }
                slot++;
            }
        }
    } else if (arg0->flags & WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT) {
        slot = arg0->context.capsule->contacts;
        for (;;) {
            if (slot->flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                if ((slot->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_GRID) {
                    arg1->vx = slot->point.vx;
                    arg1->vy = slot->point.vy;
                    arg1->vz = slot->point.vz;
                    i        = 1;
                    goto done_search;
                }
            }
            if (slot->flags & WORLD_COLLISION_CONTACT_LAST) {
                goto done_search;
            }
            slot++;
        }
    }

done_search:
    gte_SetRotMatrix(&arg0->coord->workm);
    for (; i < 2; i++) {
        src                       = &rec->ends[i];
        scratch->localEndpoint.vx = src->vx + arg0->pos.vx;
        scratch->localEndpoint.vy = src->vy + arg0->pos.vy;
        scratch->localEndpoint.vz = src->vz + arg0->pos.vz;
        gte_ldv0(&scratch->localEndpoint);
        gte_rtv0();
        gte_stlvnl(&scratch->work.rotatedEndpoint);
        arg1[i].vx = scratch->work.rotatedEndpoint.vx + (arg0->coord)->workm.t[0];
        arg1[i].vy = scratch->work.rotatedEndpoint.vy + (arg0->coord)->workm.t[1];
        arg1[i].vz = scratch->work.rotatedEndpoint.vz + (arg0->coord)->workm.t[2];
    }

    scratch->work.segmentDelta.vx = arg1[0].vx - arg1[1].vx;
    scratch->work.segmentDelta.vy = arg1[0].vy - arg1[1].vy;
    scratch->work.segmentDelta.vz = arg1[0].vz - arg1[1].vz;
    VectorNormalS(&scratch->work.segmentDelta, arg2);

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionCapsuleSegmentScratch);
}

void func_800DEF80(WorldCollisionBody* node, WorldCollisionTrigger* other)
{
    _WorldCollisionTriggerSphereScratch* scratch;
    s32                                  distSq;
    s32                                  kind;
    s32                                  dot;
    s32                                  dist;
    s32                                  tmp;
    s32                                  i;
    VECTOR *                             va, *vb;
    s16                                  faceDot;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionTriggerSphereScratch);
    Gp_ObjWorldPosInline(node, &scratch->sphereCenter);
    gte_SetRotMatrix(&other->coord->workm);
    gte_ldv0(&other->origin);
    gte_rtv0();
    gte_stlvnl(&scratch->origin);
    scratch->origin.vx += other->coord->workm.t[0];
    scratch->origin.vy += other->coord->workm.t[1];
    scratch->origin.vz += other->coord->workm.t[2];

    scratch->work.sphereToOrigin.vx = scratch->origin.vx - scratch->sphereCenter.vx;
    scratch->work.sphereToOrigin.vy = scratch->origin.vy - scratch->sphereCenter.vy;
    scratch->work.sphereToOrigin.vz = scratch->origin.vz - scratch->sphereCenter.vz;
    distSq                          = scratch->work.sphereToOrigin.vx * scratch->work.sphereToOrigin.vx + scratch->work.sphereToOrigin.vy * scratch->work.sphereToOrigin.vy +
             scratch->work.sphereToOrigin.vz * scratch->work.sphereToOrigin.vz;
    tmp = other->radius + node->radius;
    if (tmp * tmp < distSq) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionTriggerSphereScratch);
        return;
    }

    // Apply the action trigger's facing or proximity gate before the quad test.
    kind = other->flags & WORLD_COLLISION_TRIGGER_KIND_MASK;
    if (kind == WORLD_COLLISION_TRIGGER_FACING_QUAD) {
        GfxCoord* c;
        s32       m0, m1, m2, a;

        c    = node->coord;
        a    = other->facingNormal.vx;
        m0   = a * c->coord.m[0][2];
        a    = other->facingNormal.vy;
        m1   = a * c->coord.m[1][2];
        a    = other->facingNormal.vz;
        m2   = a * c->coord.m[2][2];
        dot  = m0 + m1;
        dot += m2;
        if (dot > WORLD_COLLISION_TRIGGER_FACING_DOT_MAX) {
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionTriggerSphereScratch);
            return;
        }
    } else if (kind == WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD) {
        if (distSq <= WORLD_COLLISION_TRIGGER_NEAR_DISTANCE_SQUARED_MAX) {
            other->hit = 1;
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionTriggerSphereScratch);
            return;
        }
        scratch->leveledOrigin.vx = other->origin.vx;
        scratch->leveledOrigin.vy = node->coord->coord.t[1] + node->pos.vy;
        scratch->leveledOrigin.vz = other->origin.vz;
        gte_SetRotMatrix(&other->coord->workm);
        gte_ldv0(&scratch->leveledOrigin);
        gte_rtv0();
        gte_stlvnl(&scratch->work.leveledOriginToSphere);
        scratch->work.leveledOriginToSphere.vx = scratch->sphereCenter.vx - (scratch->work.leveledOriginToSphere.vx + other->coord->workm.t[0]);
        scratch->work.leveledOriginToSphere.vy = scratch->sphereCenter.vy - (scratch->work.leveledOriginToSphere.vy + other->coord->workm.t[1]);
        scratch->work.leveledOriginToSphere.vz = scratch->sphereCenter.vz - (scratch->work.leveledOriginToSphere.vz + other->coord->workm.t[2]);
        VectorNormal(&scratch->work.leveledOriginToSphere, &scratch->work.leveledOriginToSphere);
        {
            GfxCoord* c;
            s32       n0, n1, n2;

            c    = node->coord;
            n0   = scratch->work.leveledOriginToSphere.vx * c->workm.m[0][2];
            n1   = scratch->work.leveledOriginToSphere.vy * c->workm.m[1][2];
            n2   = scratch->work.leveledOriginToSphere.vz * c->workm.m[2][2];
            dot  = n0 + n1;
            dot += n2;
        }
        if (dot > WORLD_COLLISION_TRIGGER_FACING_DOT_MAX) {
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionTriggerSphereScratch);
            return;
        }
    }

    // Test the sphere against the transformed quad's one-sided plane and edges.
    gte_ldv0(&other->vertices[0]);
    gte_rtv0();
    gte_stlvnl(&scratch->corners[0]);
    scratch->corners[0].vx += scratch->origin.vx;
    scratch->corners[0].vy += scratch->origin.vy;
    scratch->corners[0].vz += scratch->origin.vz;

    gte_ldv0(&other->normal);
    gte_rtv0();
    gte_stlvnl(&scratch->faceNormal);

    faceDot = (scratch->faceNormal.vx * scratch->corners[0].vx + scratch->faceNormal.vy * scratch->corners[0].vy +
               scratch->faceNormal.vz * scratch->corners[0].vz) >>
              12;
    dist = ((scratch->faceNormal.vx * scratch->sphereCenter.vx + scratch->faceNormal.vy * scratch->sphereCenter.vy +
             scratch->faceNormal.vz * scratch->sphereCenter.vz) >>
            12) -
           faceDot;
    if (dist >= 0 || dist < -node->radius) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionTriggerSphereScratch);
        return;
    }

    for (i = 1; i < (s32)ARRAY_SIZE(other->vertices); i++) {
        gte_ldv0(&other->vertices[i]);
        gte_rtv0();
        gte_stlvnl(&scratch->corners[i]);
        scratch->corners[i].vx += scratch->origin.vx;
        scratch->corners[i].vy += scratch->origin.vy;
        scratch->corners[i].vz += scratch->origin.vz;
    }

    for (i = 1; i < (s32)ARRAY_SIZE(other->vertices) + 1; i++) {
        va                                = &scratch->corners[(u16)Gp_FaceEdgePairs[i].endCornerIndex];
        vb                                = &scratch->corners[(u16)Gp_FaceEdgePairs[i].startCornerIndex];
        scratch->work.edgeDisplacement.vx = va->vx - vb->vx;
        scratch->work.edgeDisplacement.vy = va->vy - vb->vy;
        scratch->work.edgeDisplacement.vz = va->vz - vb->vz;
        gte_ldopv1(&scratch->faceNormal);
        gte_ldopv2(&scratch->work.edgeDisplacement);
        gte_op12();
        gte_stlvnl(&scratch->edgePlaneNormal);
        tmp   = scratch->edgePlaneNormal.vx * scratch->sphereCenter.vx + scratch->edgePlaneNormal.vy * scratch->sphereCenter.vy;
        tmp  += scratch->edgePlaneNormal.vz * scratch->sphereCenter.vz;
        tmp >>= 12;
        tmp  -= (scratch->edgePlaneNormal.vx * va->vx + scratch->edgePlaneNormal.vy * va->vy + scratch->edgePlaneNormal.vz * va->vz) >> 12;
        if (tmp >= 0) {
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionTriggerSphereScratch);
            return;
        }
    }

    other->hit = 1;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionTriggerSphereScratch);
}

void func_800DF6AC(WorldCollisionBody* node, WorldCollisionTrigger* other, VECTOR3* from)
{
    _WorldCollisionViewBoundarySphereScratch* scratch;
    s32                                       dist;
    s32                                       tmp;
    s32                                       i;
    VECTOR *                                  va, *vb;
    s16                                       faceDot;

    scratch                       = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
    scratch->movementDirection.vx = node->coord->coord.t[0] - from->vx;
    scratch->movementDirection.vy = node->coord->coord.t[1] - from->vy;
    scratch->movementDirection.vz = node->coord->coord.t[2] - from->vz;
    SquareRoot0(scratch->movementDirection.vx * scratch->movementDirection.vx + scratch->movementDirection.vy * scratch->movementDirection.vy + scratch->movementDirection.vz * scratch->movementDirection.vz);
    VectorNormal(&scratch->movementDirection, &scratch->movementDirection);
    // A view boundary accepts only movement against its room-space normal.
    if (other->normal.vx * scratch->movementDirection.vx + other->normal.vy * scratch->movementDirection.vy + other->normal.vz * scratch->movementDirection.vz >=
        0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
        return;
    }

    Gp_ObjWorldPosInline(node, &scratch->sphereCenter);
    gte_SetRotMatrix(&other->coord->workm);
    gte_ldv0(&other->origin);
    gte_rtv0();
    gte_stlvnl(&scratch->origin);
    scratch->origin.vx += other->coord->workm.t[0];
    scratch->origin.vy += other->coord->workm.t[1];
    scratch->origin.vz += other->coord->workm.t[2];

    scratch->work.sphereToOrigin.vx = scratch->origin.vx - scratch->sphereCenter.vx;
    scratch->work.sphereToOrigin.vy = scratch->origin.vy - scratch->sphereCenter.vy;
    scratch->work.sphereToOrigin.vz = scratch->origin.vz - scratch->sphereCenter.vz;
    tmp                             = other->radius + node->radius;
    if (tmp * tmp < scratch->work.sphereToOrigin.vx * scratch->work.sphereToOrigin.vx + scratch->work.sphereToOrigin.vy * scratch->work.sphereToOrigin.vy +
                        scratch->work.sphereToOrigin.vz * scratch->work.sphereToOrigin.vz) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
        return;
    }

    gte_ldv0(&other->vertices[0]);
    gte_rtv0();
    gte_stlvnl(&scratch->corners[0]);
    scratch->corners[0].vx += scratch->origin.vx;
    scratch->corners[0].vy += scratch->origin.vy;
    scratch->corners[0].vz += scratch->origin.vz;

    gte_ldv0(&other->normal);
    gte_rtv0();
    gte_stlvnl(&scratch->faceNormal);

    faceDot = (scratch->faceNormal.vx * scratch->corners[0].vx + scratch->faceNormal.vy * scratch->corners[0].vy +
               scratch->faceNormal.vz * scratch->corners[0].vz) >>
              12;
    dist = ((scratch->faceNormal.vx * scratch->sphereCenter.vx + scratch->faceNormal.vy * scratch->sphereCenter.vy +
             scratch->faceNormal.vz * scratch->sphereCenter.vz) >>
            12) -
           faceDot;
    if (dist >= 0 || dist < -node->radius) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
        return;
    }

    for (i = 1; i < (s32)ARRAY_SIZE(other->vertices); i++) {
        gte_ldv0(&other->vertices[i]);
        gte_rtv0();
        gte_stlvnl(&scratch->corners[i]);
        scratch->corners[i].vx += scratch->origin.vx;
        scratch->corners[i].vy += scratch->origin.vy;
        scratch->corners[i].vz += scratch->origin.vz;
    }

    for (i = 1; i < (s32)ARRAY_SIZE(other->vertices) + 1; i++) {
        va                                = &scratch->corners[(u16)Gp_FaceEdgePairs[i].endCornerIndex];
        vb                                = &scratch->corners[(u16)Gp_FaceEdgePairs[i].startCornerIndex];
        scratch->work.edgeDisplacement.vx = va->vx - vb->vx;
        scratch->work.edgeDisplacement.vy = va->vy - vb->vy;
        scratch->work.edgeDisplacement.vz = va->vz - vb->vz;
        gte_ldopv1(&scratch->faceNormal);
        gte_ldopv2(&scratch->work.edgeDisplacement);
        gte_op12();
        gte_stlvnl(&scratch->edgePlaneNormal);
        tmp   = scratch->edgePlaneNormal.vx * scratch->sphereCenter.vx + scratch->edgePlaneNormal.vy * scratch->sphereCenter.vy;
        tmp  += scratch->edgePlaneNormal.vz * scratch->sphereCenter.vz;
        tmp >>= 12;
        tmp  -= (scratch->edgePlaneNormal.vx * va->vx + scratch->edgePlaneNormal.vy * va->vy + scratch->edgePlaneNormal.vz * va->vz) >> 12;
        if (tmp >= 0) {
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
            return;
        }
    }

    other->hit = 1;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
}

s32 func_800DFCCC(WorldCollisionOccluder* occluder, SVECTOR* arg1, SVECTOR* arg2, VECTOR* arg3)
{
    _WorldCollisionOccluderSegmentScratch* block;
    VECTOR*                                va;
    VECTOR*                                vb;
    s32                                    dirDot;
    s32                                    t;
    s32                                    hitDot;
    s32                                    i;
    s16                                    planeDot;
    s16                                    edgeDot;

    block = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionOccluderSegmentScratch);

    // Transform the occluder quad into the segment's query space.
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&occluder->origin);
    gte_rtv0();
    gte_stlvnl(&block->position.origin);
    block->position.origin.vx += gGfxViewCoord.workm.t[0];
    block->position.origin.vy += gGfxViewCoord.workm.t[1];
    block->position.origin.vz += gGfxViewCoord.workm.t[2];

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&occluder->vertices[0]);
    gte_rtv0();
    gte_stlvnl(&block->corners[0]);
    block->corners[0].vx += block->position.origin.vx;
    block->corners[0].vy += block->position.origin.vy;
    block->corners[0].vz += block->position.origin.vz;

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&occluder->normal);
    gte_rtv0();
    gte_stlvnl(&block->faceNormal);

    planeDot = (block->faceNormal.vx * block->corners[0].vx + block->faceNormal.vy * block->corners[0].vy +
                block->faceNormal.vz * block->corners[0].vz) >>
               12;
    dirDot = (block->faceNormal.vx * arg3->vx + block->faceNormal.vy * arg3->vy + block->faceNormal.vz * arg3->vz) >> 12;
    if (dirDot == 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);
        return 0;
    }
    t = ((block->faceNormal.vx * arg1->vx + block->faceNormal.vy * arg1->vy + block->faceNormal.vz * arg1->vz) >> 12) -
        planeDot;
    t = -(t << 12) / dirDot;
    if (t == 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);
        return 0;
    }

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    for (i = 1; i < (s32)ARRAY_SIZE(occluder->vertices); i++) {
        gte_ldv0(&occluder->vertices[i]);
        gte_rtv0();
        gte_stlvnl(&block->corners[i]);
        block->corners[i].vx += block->position.origin.vx;
        block->corners[i].vy += block->position.origin.vy;
        block->corners[i].vz += block->position.origin.vz;
    }

    block->position.intersection.vx = arg1->vx + ((arg3->vx * t) >> 12);
    block->position.intersection.vy = arg1->vy + ((arg3->vy * t) >> 12);
    block->position.intersection.vz = arg1->vz + ((arg3->vz * t) >> 12);

    // An intersection at either endpoint does not block the segment.
    if ((block->position.intersection.vx - arg1->vx) * (block->position.intersection.vx - arg2->vx) +
            (block->position.intersection.vy - arg1->vy) * (block->position.intersection.vy - arg2->vy) +
            (block->position.intersection.vz - arg1->vz) * (block->position.intersection.vz - arg2->vz) >=
        0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);
        return 0;
    }

    for (i = 1; i < (s32)ARRAY_SIZE(occluder->vertices) + 1; i++) {
        va                         = &block->corners[(u16)Gp_FaceEdgePairs[i].endCornerIndex];
        vb                         = &block->corners[(u16)Gp_FaceEdgePairs[i].startCornerIndex];
        block->edgeDisplacement.vx = va->vx - vb->vx;
        block->edgeDisplacement.vy = va->vy - vb->vy;
        block->edgeDisplacement.vz = va->vz - vb->vz;
        gte_ldopv1(&block->faceNormal);
        gte_ldopv2(&block->edgeDisplacement);
        gte_op12();
        gte_stlvnl(&block->edgePlaneNormal);
        edgeDot = (block->edgePlaneNormal.vx * va->vx + block->edgePlaneNormal.vy * va->vy +
                   block->edgePlaneNormal.vz * va->vz) >>
                  12;
        hitDot = (block->edgePlaneNormal.vx * block->position.intersection.vx +
                  block->edgePlaneNormal.vy * block->position.intersection.vy +
                  block->edgePlaneNormal.vz * block->position.intersection.vz) >>
                 12;
        if (hitDot - edgeDot > 0) {
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);
            return 0;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);
    return 1;
}
