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

/// Fractional bits of unit collision directions and normals (4096 per unit).
enum { WORLD_COLLISION_DIRECTION_FRACTION_BITS = 12 };

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

static void _worldCollisionMarkMotionSphereGridCandidates(const WorldCollisionBody* body);

static void _worldCollisionMarkCapsuleGridCandidates(const WorldCollisionBody* body);

static void _worldCollisionMarkGridFaceCandidates(VECTOR gridEndpoints[2], s32 unusedSelector);

static void _worldCollisionMarkViewSegmentCandidates(const SVECTOR* target, const SVECTOR* start);

/// Places one body-local footprint endpoint in biased collision-grid coordinates.
///
/// scratch borrows a live query block whose selected signed-halfword endpoint
/// and bodyToRoom transform are initialized. endpointIndex is 0 or 1, as selected
/// by both callers' two-element loops. The GTE rotation matrix must already be
/// bodyToRoom and the active grid must be live. Adds full-word room translation
/// and grid bias to rotated X/Z, forces Y to zero and leaves the fourth vector
/// word untouched. Coordinates use game units; the input endpoint is unchanged.
static __inline__ void _worldCollisionPlaceBodyGridEndpoint(_WorldCollisionGridBodyQueryScratch* scratch, s32 endpointIndex)
{
    gte_ldv0(&scratch->localEndpoints[endpointIndex]);
    gte_rtv0();
    gte_stlvnl(&scratch->gridEndpoints[endpointIndex]);
    scratch->gridEndpoints[endpointIndex].vx = scratch->gridEndpoints[endpointIndex].vx + scratch->bodyToRoom.t[0] + Gp_GridParams->xBias;
    scratch->gridEndpoints[endpointIndex].vy = 0;
    scratch->gridEndpoints[endpointIndex].vz = scratch->gridEndpoints[endpointIndex].vz + scratch->bodyToRoom.t[2] + Gp_GridParams->zBias;
}

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
    _worldCollisionMarkMotionSphereGridCandidates(arg0);
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

/// Accumulates grid-face candidates along a motion sphere's radius-scaled XZ footprint.
///
/// Requires a kind-4 body's live motion context, composed body/view matrices,
/// an active grid and an initialized scratch stack with 120 free bytes.
/// Positions and radius use game units; the motion direction uses 4096 per unit.
/// The existing arithmetic adds its XZ components to the local body position
/// before transforming; the intended frame relationship is unproven.
/// The caller clears `D_80115450`; this query only sets marks and retains no pointers.
static void _worldCollisionMarkMotionSphereGridCandidates(const WorldCollisionBody* body)
{
    s32                                  endpointIndex;
    _WorldCollisionGridBodyQueryScratch* scratch;
    const SVECTOR*                       motionDirection;
    MATRIX*                              bodyToRoom;

    // Keep halfword truncation and the asymmetric rounding of the negative end.
    motionDirection               = &body->context.motion->motionDirection;
    scratch                       = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionGridBodyQueryScratch);
    bodyToRoom                    = &scratch->bodyToRoom;
    scratch->localEndpoints[0].vx = (u16)body->pos.vx + ((motionDirection->vx * body->radius) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS);
    scratch->localEndpoints[0].vy = 0;
    scratch->localEndpoints[0].vz = (u16)body->pos.vz + ((motionDirection->vz * body->radius) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS);
    scratch->localEndpoints[1].vx = (u16)body->pos.vx + (-(motionDirection->vx * body->radius) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS);
    scratch->localEndpoints[1].vy = 0;
    scratch->localEndpoints[1].vz = (u16)body->pos.vz + (-(motionDirection->vz * body->radius) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS);
    // Remove the composed view transform and place the footprint in the biased grid.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &body->coord->workm, bodyToRoom);
    gte_SetRotMatrix(bodyToRoom);
    for (endpointIndex = 0; endpointIndex < (s32)ARRAY_SIZE(scratch->localEndpoints); endpointIndex++) {
        _worldCollisionPlaceBodyGridEndpoint(scratch, endpointIndex);
    }
    _worldCollisionMarkGridFaceCandidates(scratch->gridEndpoints, 0);
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

    _worldCollisionMarkCapsuleGridCandidates(obj);
    worldCollisionPlaceCapsuleSegment(obj, scratch->endpoints, scratch->ray, WORLD_COLLISION_CAPSULE_SEGMENT_GRID_SCAN);

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

/// Accumulates grid-face candidates along a capsule body's XZ endpoint segment.
///
/// Requires a kind-3 body's live capsule, composed body/view matrices, an active
/// grid and an initialized scratch stack with 120 free bytes. Endpoint offsets
/// plus the local body position are truncated to signed halfwords, in game units,
/// before conversion to biased-grid XZ. Y is ignored. The caller clears
/// `D_80115450`; this query only sets marks and retains no pointers.
static void _worldCollisionMarkCapsuleGridCandidates(const WorldCollisionBody* body)
{
    s32                                  endpointIndex;
    _WorldCollisionGridBodyQueryScratch* scratch;
    const SVECTOR*                       endOffsets;
    GfxCoord*                            bodyCoord;
    MATRIX*                              bodyToRoom;

    bodyCoord  = body->coord;
    scratch    = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionGridBodyQueryScratch);
    bodyToRoom = &scratch->bodyToRoom;
    endOffsets = body->context.capsule->ends;
    // Place the two body-local offsets in biased-grid XZ coordinates.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &bodyCoord->workm, bodyToRoom);
    gte_SetRotMatrix(bodyToRoom);
    for (endpointIndex = 0; endpointIndex < (s32)ARRAY_SIZE(scratch->localEndpoints); endpointIndex++) {
        scratch->localEndpoints[endpointIndex].vx = (u16)endOffsets[endpointIndex].vx + (u16)body->pos.vx;
        scratch->localEndpoints[endpointIndex].vy = 0;
        scratch->localEndpoints[endpointIndex].vz = (u16)endOffsets[endpointIndex].vz + (u16)body->pos.vz;
        _worldCollisionPlaceBodyGridEndpoint(scratch, endpointIndex);
    }
    _worldCollisionMarkGridFaceCandidates(scratch->gridEndpoints, 1);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridBodyQueryScratch);
}

/// Accumulates face marks from grid cells near two biased-grid XZ endpoints.
///
/// `gridEndpoints` supplies two writable VECTORs in game units; only X/Z are
/// read or changed. A nonzero normalized direction extends both ends by the
/// approximate half-cell diagonal. A zero direction uses a point-radius test.
/// The caller clears `D_80115450`; shared faces are simply marked again.
/// Cell lists must end in WORLD_COLLISION_GRID_CELL_END and contain indices in
/// [0, faceCount), with faceCount at most ARRAY_SIZE(D_80115450). Cell centres
/// and distance-test displacements narrow to signed halfwords. Nonzero segment
/// deltas need signed-halfword components and squared length in 1..0x7FFFFFFF;
/// a zero delta is allowed and selects the point test.
/// Requires a live active grid and 40 free bytes on the initialized scratch
/// stack; changes GTE state, releases that block and retains no pointers.
/// `unusedSelector` is ignored; callers pass 0 or 1 and its intended role is unproven.
static void _worldCollisionMarkGridFaceCandidates(VECTOR gridEndpoints[2], s32 unusedSelector)
{
    enum {
        WORLD_COLLISION_GRID_DIAGONAL_SCALE_Q7      = 0xB5,
        WORLD_COLLISION_GRID_DIAGONAL_FRACTION_BITS = 7,
        WORLD_COLLISION_GRID_FACE_CANDIDATE         = 1
    };
    u8*                                  scratchCursor;
    _WorldCollisionGridCandidateScratch* scratch;
    WorldCollisionGrid*                  pointGrid;
    WorldCollisionGrid*                  segmentGrid;
    s32                                  segmentRadiusSquared;
    u32                                  cellSize;
    s32                                  halfCellSize;
    s32                                  searchRadius;
    s32                                  pointRadiusSquared;
    s32                                  cellX;
    s32                                  cellZ;
    s32                                  targetSideQ12;
    s32                                  startProjection;
    s32                                  targetZ;
    s32                                  startZ;
    s16*                                 faceIndices;
    s16                                  faceIndex;

    /// Marks every face in one sentinel-terminated grid-cell list.
    ///
    /// `faceIndicesArg` must be a writable s16* local pointing to valid face
    /// indices followed by WORLD_COLLISION_GRID_CELL_END. It is evaluated
    /// repeatedly and advanced to the sentinel. Captures the signed-halfword
    /// `faceIndex`, WORLD_COLLISION_GRID_FACE_CANDIDATE and `D_80115450`;
    /// sets marks without clearing any or retaining pointers. The loop expands
    /// as a standalone statement within this query's two cell-scan branches.
#define WORLD_COLLISION_MARK_CELL_FACE_CANDIDATES(faceIndicesArg)    \
    while (*(faceIndicesArg) != WORLD_COLLISION_GRID_CELL_END) {     \
        faceIndex             = *(faceIndicesArg);                   \
        D_80115450[faceIndex] = WORLD_COLLISION_GRID_FACE_CANDIDATE; \
        (faceIndicesArg)++;                                          \
    }

    scratchCursor            = SCRATCH_STACK_CURSOR(u8);
    cellSize                 = Gp_GridParams->cellSize;
    scratch                  = (_WorldCollisionGridCandidateScratch*)(SCRATCH_STACK_CURSOR(void) = scratchCursor - sizeof(*scratch));
    scratch->segmentDelta.vx = gridEndpoints[0].vx - gridEndpoints[1].vx;
    scratch->segmentDelta.vy = 0;
    targetZ                  = gridEndpoints[0].vz;
    startZ                   = gridEndpoints[1].vz;
    scratch->segmentDelta.vz = targetZ - startZ;
    halfCellSize             = cellSize >> 1;
    // Expand the query footprint by the cell's approximate half diagonal.
    searchRadius = ((halfCellSize * WORLD_COLLISION_GRID_DIAGONAL_SCALE_Q7) >> WORLD_COLLISION_GRID_DIAGONAL_FRACTION_BITS) + 1;
    VectorNormalS(&scratch->segmentDelta, &scratch->segmentDirection);

    if ((scratch->segmentDirection.vx == 0) && (scratch->segmentDirection.vz == 0)) {
        for (cellX = 0; cellX < Gp_GridParams->cellCountX; cellX++) {
            pointRadiusSquared = searchRadius * searchRadius;
            for (cellZ = 0; cellZ < Gp_GridParams->cellCountZ; cellZ++) {
                pointGrid                = Gp_GridParams;
                scratch->cellCenter.vx   = cellX * pointGrid->cellSize + (pointGrid->cellSize >> 1);
                scratch->cellCenter.vz   = cellZ * pointGrid->cellSize + (pointGrid->cellSize >> 1);
                scratch->displacement.vx = (u16)scratch->cellCenter.vx - (u16)gridEndpoints[0].vx;
                scratch->displacement.vz = (u16)scratch->cellCenter.vz - (u16)gridEndpoints[0].vz;
                if ((scratch->displacement.vx * scratch->displacement.vx) +
                        (scratch->displacement.vz * scratch->displacement.vz) <
                    pointRadiusSquared) {
                    faceIndices = pointGrid->cellFaceIds[cellX * pointGrid->cellCountZ + cellZ];
                    if (faceIndices != NULL) {
                        // Cell lists select face candidates; their indices are signed.
                        WORLD_COLLISION_MARK_CELL_FACE_CANDIDATES(faceIndices);
                    }
                }
            }
        }
    } else {
        // Extend both endpoints before reusing the displacement for cell-distance tests.
        scratch->displacement.vx = (scratch->segmentDirection.vx * searchRadius) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS;
        scratch->displacement.vz = (scratch->segmentDirection.vz * searchRadius) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS;
        gridEndpoints[0].vx     += scratch->displacement.vx;
        gridEndpoints[0].vz     += scratch->displacement.vz;
        gridEndpoints[1].vx     -= scratch->displacement.vx;
        gridEndpoints[1].vz     -= scratch->displacement.vz;
        for (cellX = 0; cellX < Gp_GridParams->cellCountX; cellX++) {
            segmentRadiusSquared = searchRadius * searchRadius;
            for (cellZ = 0; cellZ < Gp_GridParams->cellCountZ; cellZ++) {
                segmentGrid            = Gp_GridParams;
                scratch->cellCenter.vx = cellX * segmentGrid->cellSize + (segmentGrid->cellSize >> 1);
                scratch->cellCenter.vz = cellZ * segmentGrid->cellSize + (segmentGrid->cellSize >> 1);
                targetSideQ12          = ((scratch->cellCenter.vx - gridEndpoints[0].vx) * scratch->segmentDirection.vx) +
                                ((scratch->cellCenter.vz - gridEndpoints[0].vz) * scratch->segmentDirection.vz);
                if (targetSideQ12 <= 0) {
                    startProjection = (((scratch->cellCenter.vx - gridEndpoints[1].vx) * scratch->segmentDirection.vx) +
                                       ((scratch->cellCenter.vz - gridEndpoints[1].vz) * scratch->segmentDirection.vz)) >>
                                      WORLD_COLLISION_DIRECTION_FRACTION_BITS;
                    if (startProjection > 0) {
                        scratch->displacement.vx = ((u16)gridEndpoints[1].vx + ((scratch->segmentDirection.vx * startProjection) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS)) -
                                                   (u16)scratch->cellCenter.vx;
                        scratch->displacement.vz = ((u16)gridEndpoints[1].vz + ((scratch->segmentDirection.vz * startProjection) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS)) -
                                                   (u16)scratch->cellCenter.vz;
                        if ((scratch->displacement.vx * scratch->displacement.vx) +
                                (scratch->displacement.vz * scratch->displacement.vz) <
                            segmentRadiusSquared) {
                            faceIndices = segmentGrid->cellFaceIds[cellX * segmentGrid->cellCountZ + cellZ];
                            if (faceIndices != NULL) {
                                WORLD_COLLISION_MARK_CELL_FACE_CANDIDATES(faceIndices);
                            }
                        }
                    }
                }
            }
        }
    }

#undef WORLD_COLLISION_MARK_CELL_FACE_CANDIDATES

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridCandidateScratch);
}

s32 worldCollisionProbeGridSegment(const SVECTOR* target, const SVECTOR* start, SVECTOR* hitPoint, SVECTOR* surfaceNormal)
{
    WorldCollisionGrid*              grid;
    s32                              hit;
    _WorldCollisionGridProbeScratch* scratch;
    s32                              faceIndex;

    grid = Gp_GridParams;
    hit  = 0;
    if (grid == NULL) {
        return hit;
    }

    // Build the candidate mask before testing and clipping blocking faces.
    faceIndex = 0;
    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionGridProbeScratch);
    if (hit < grid->faceCount) {
        do {
            D_80115450[faceIndex] = 0;
            faceIndex++;
        } while (faceIndex < Gp_GridParams->faceCount);
    }
    _worldCollisionMarkViewSegmentCandidates(target, start);
    // Keep the direction fixed as accepted intersections shorten the segment.
    scratch->endpoints[0].vx = target->vx;
    scratch->endpoints[0].vy = target->vy;
    scratch->endpoints[0].vz = target->vz;
    scratch->endpoints[1].vx = start->vx;
    scratch->endpoints[1].vy = start->vy;
    scratch->endpoints[1].vz = start->vz;
    scratch->delta.vx        = scratch->endpoints[0].vx - scratch->endpoints[1].vx;
    scratch->delta.vy        = scratch->endpoints[0].vy - scratch->endpoints[1].vy;
    scratch->delta.vz        = scratch->endpoints[0].vz - scratch->endpoints[1].vz;
    VectorNormalS(&scratch->delta, &scratch->ray[0]);
    for (faceIndex = 0; faceIndex < Gp_GridParams->faceCount; faceIndex++) {
        if (D_80115450[faceIndex] == 0) {
            continue;
        }
        if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1]
                              [Gp_GridParams->faces[faceIndex].surfaceClass]
                                  ->probePassThrough != WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
            continue;
        }
        if (worldCollisionIntersectGridFace(faceIndex, scratch->endpoints, scratch->ray, NULL) == 0) {
            continue;
        }
        if (hitPoint != NULL) {
            hitPoint->vx = scratch->ray[1].vx;
            hitPoint->vy = scratch->ray[1].vy;
            hitPoint->vz = scratch->ray[1].vz;
        }
        if (surfaceNormal != NULL) {
            surfaceNormal->vx = Gp_GridParams->normals[Gp_GridParams->faces[faceIndex].normalIndex].vx;
            surfaceNormal->vy = Gp_GridParams->normals[Gp_GridParams->faces[faceIndex].normalIndex].vy;
            surfaceNormal->vz = Gp_GridParams->normals[Gp_GridParams->faces[faceIndex].normalIndex].vz;
        }
        scratch->endpoints[0].vx = scratch->ray[1].vx;
        scratch->endpoints[0].vy = scratch->ray[1].vy;
        scratch->endpoints[0].vz = scratch->ray[1].vz;
        hit                      = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridProbeScratch);
    return hit;
}

/// Accumulates grid-face candidates along a view-space segment's XZ projection.
///
/// Endpoints use signed-halfword game coordinates. Requires an active grid
/// bound to the composed view transform, a caller-cleared `D_80115450` and
/// 104 free bytes on the initialized scratch stack. Conversion applies the
/// inverse view rotation, grid bias and local view translation subtraction,
/// then truncates X/Z to signed halfwords. Inputs are unchanged; temporary grid
/// endpoints may be extended by the scan. Releases its block and retains no pointers.
static void _worldCollisionMarkViewSegmentCandidates(const SVECTOR* target, const SVECTOR* start)
{
    _WorldCollisionGridQueryScratch* scratch;
    VECTOR*                          rotatedEndpoint;

    /// Rotates one staged view endpoint into biased-grid XZ coordinates.
    ///
    /// Uses the active grid's bound view transform and writes only grid XYZ,
    /// truncating X/Z to signed halfwords and setting Y to zero. Arguments must
    /// be side-effect-free scratch/vector identifiers and endpoint index 0 or 1;
    /// they are evaluated repeatedly. Captures `Gp_GridParams`, changes GTE state,
    /// and expands to a statement sequence used only as a standalone phase here.
#define WORLD_COLLISION_PLACE_VIEW_GRID_ENDPOINT(scratchArg, rotatedArg, endpointIndex)                                                      \
    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &(scratchArg)->viewEndpoint, (rotatedArg));                                     \
    {                                                                                                                                        \
        WorldCollisionGrid* grid = Gp_GridParams;                                                                                            \
                                                                                                                                             \
        (scratchArg)->gridEndpoints[endpointIndex].vx = (s16)((scratchArg)->rotatedEndpoint.vx + grid->xBias - grid->viewCoord->coord.t[0]); \
        (scratchArg)->gridEndpoints[endpointIndex].vy = 0;                                                                                   \
        (scratchArg)->gridEndpoints[endpointIndex].vz = (s16)((scratchArg)->rotatedEndpoint.vz + grid->zBias - grid->viewCoord->coord.t[2]); \
    }

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionGridQueryScratch);
    // Convert each view endpoint to biased-grid XZ coordinates before scanning.
    scratch->viewEndpoint.vx = target->vx;
    scratch->viewEndpoint.vy = target->vy;
    scratch->viewEndpoint.vz = target->vz;
    rotatedEndpoint          = &scratch->rotatedEndpoint;
    WORLD_COLLISION_PLACE_VIEW_GRID_ENDPOINT(scratch, rotatedEndpoint, 0);
    scratch->viewEndpoint.vx = start->vx;
    scratch->viewEndpoint.vy = start->vy;
    scratch->viewEndpoint.vz = start->vz;
    WORLD_COLLISION_PLACE_VIEW_GRID_ENDPOINT(scratch, rotatedEndpoint, 1);
#undef WORLD_COLLISION_PLACE_VIEW_GRID_ENDPOINT

    _worldCollisionMarkGridFaceCandidates(scratch->gridEndpoints, 0);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionGridQueryScratch);
}

/// Promotes a contact's signed-halfword XYZ into a full-width position.
///
/// Preserves the contact's coordinate frame and leaves the output pad word
/// untouched. Both borrowed objects must be live and disjoint.
static inline void _worldCollisionCopyContactPoint(VECTOR* positionOut, const WorldCollisionContact* contact)
{
    positionOut->vx = contact->point.vx;
    positionOut->vy = contact->point.vy;
    positionOut->vz = contact->point.vz;
}

void worldCollisionPlaceCapsuleSegment(const WorldCollisionBody* body, VECTOR endpoints[2], SVECTOR* direction, s32 gridScan)
{
    _WorldCollisionCapsuleSegmentScratch* scratch;
    const WorldCollisionCapsule*          capsule;
    const SVECTOR*                        localEnd;
    const WorldCollisionContact*          contact;
    s32                                   flags;
    s32                                   endpointIndex;

    capsule       = body->context.capsule;
    scratch       = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionCapsuleSegmentScratch);
    endpointIndex = 0;

    // A retained contact may replace endpoint 0 without transforming it again.
    if (gridScan == WORLD_COLLISION_CAPSULE_SEGMENT_PAIR_TEST) {
        if (body->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {
            contact = body->context.capsule->contacts;
            for (;;) {
                flags = contact->flags;
                if (flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                    _worldCollisionCopyContactPoint(endpoints, contact);
                    endpointIndex = 1;
                    break;
                }
                if (flags & WORLD_COLLISION_CONTACT_LAST) {
                    break;
                }
                contact++;
            }
        } else if (body->flags & WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT) {
            contact = body->context.capsule->contacts;
            for (;;) {
                if (contact->flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                    if ((contact->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_GRID) {
                        _worldCollisionCopyContactPoint(endpoints, contact);
                        endpointIndex = 1;
                        break;
                    }
                }
                if (contact->flags & WORLD_COLLISION_CONTACT_LAST) {
                    break;
                }
                contact++;
            }
        }
    } else if (body->flags & WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT) {
        contact = body->context.capsule->contacts;
        for (;;) {
            if (contact->flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                if ((contact->key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_GRID) {
                    _worldCollisionCopyContactPoint(endpoints, contact);
                    endpointIndex = 1;
                    break;
                }
            }
            if (contact->flags & WORLD_COLLISION_CONTACT_LAST) {
                break;
            }
            contact++;
        }
    }

    // Place the remaining local endpoints through the composed body transform.
    gte_SetRotMatrix(&body->coord->workm);
    for (; endpointIndex < (s32)ARRAY_SIZE(capsule->ends); endpointIndex++) {
        localEnd                  = &capsule->ends[endpointIndex];
        scratch->localEndpoint.vx = localEnd->vx + body->pos.vx;
        scratch->localEndpoint.vy = localEnd->vy + body->pos.vy;
        scratch->localEndpoint.vz = localEnd->vz + body->pos.vz;
        gte_ldv0(&scratch->localEndpoint);
        gte_rtv0();
        gte_stlvnl(&scratch->work.rotatedEndpoint);
        endpoints[endpointIndex].vx = scratch->work.rotatedEndpoint.vx + body->coord->workm.t[0];
        endpoints[endpointIndex].vy = scratch->work.rotatedEndpoint.vy + body->coord->workm.t[1];
        endpoints[endpointIndex].vz = scratch->work.rotatedEndpoint.vz + body->coord->workm.t[2];
    }

    // The axis points back from endpoint 1 towards the possibly clipped end.
    scratch->work.segmentDelta.vx = endpoints[0].vx - endpoints[1].vx;
    scratch->work.segmentDelta.vy = endpoints[0].vy - endpoints[1].vy;
    scratch->work.segmentDelta.vz = endpoints[0].vz - endpoints[1].vz;
    VectorNormalS(&scratch->work.segmentDelta, direction);

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

void worldCollisionTestViewBoundarySphere(const WorldCollisionBody* body, WorldCollisionTrigger* boundary, const VECTOR3* previousRootPosition)
{
    enum { WORLD_COLLISION_VIEW_BOUNDARY_HIT = 1 };
    _WorldCollisionViewBoundarySphereScratch* scratch;
    s32                                       planeDistance;
    s32                                       overlapMeasure;
    s32                                       edgeIndex;
    VECTOR *                                  edgeEnd, *edgeStart;
    s16                                       planeOffset;

    scratch                       = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
    scratch->movementDirection.vx = body->coord->coord.t[0] - previousRootPosition->vx;
    scratch->movementDirection.vy = body->coord->coord.t[1] - previousRootPosition->vy;
    scratch->movementDirection.vz = body->coord->coord.t[2] - previousRootPosition->vz;
    // Retain the original square-root call even though its result is discarded.
    SquareRoot0(scratch->movementDirection.vx * scratch->movementDirection.vx + scratch->movementDirection.vy * scratch->movementDirection.vy + scratch->movementDirection.vz * scratch->movementDirection.vz);
    VectorNormal(&scratch->movementDirection, &scratch->movementDirection);
    // A view boundary accepts only movement against its room-space normal.
    if (boundary->normal.vx * scratch->movementDirection.vx + boundary->normal.vy * scratch->movementDirection.vy + boundary->normal.vz * scratch->movementDirection.vz >=
        0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
        return;
    }

    Gp_ObjWorldPosInline(body, &scratch->sphereCenter);
    gte_SetRotMatrix(&boundary->coord->workm);
    gte_ldv0(&boundary->origin);
    gte_rtv0();
    gte_stlvnl(&scratch->origin);
    scratch->origin.vx += boundary->coord->workm.t[0];
    scratch->origin.vy += boundary->coord->workm.t[1];
    scratch->origin.vz += boundary->coord->workm.t[2];

    scratch->work.sphereToOrigin.vx = scratch->origin.vx - scratch->sphereCenter.vx;
    scratch->work.sphereToOrigin.vy = scratch->origin.vy - scratch->sphereCenter.vy;
    scratch->work.sphereToOrigin.vz = scratch->origin.vz - scratch->sphereCenter.vz;
    overlapMeasure                  = boundary->radius + body->radius;
    if (overlapMeasure * overlapMeasure < scratch->work.sphereToOrigin.vx * scratch->work.sphereToOrigin.vx + scratch->work.sphereToOrigin.vy * scratch->work.sphereToOrigin.vy +
                                              scratch->work.sphereToOrigin.vz * scratch->work.sphereToOrigin.vz) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
        return;
    }

    /// Rejects a boundary sphere outside the quad's negative-side plane or strict edges.
    ///
    /// Requires a placed origin and sphere centre, and the boundary's rotation
    /// already loaded into the GTE. Captures `planeDistance`, `overlapMeasure`,
    /// `edgeIndex`, `edgeEnd`, `edgeStart` and the signed-halfword `planeOffset`.
    /// Arguments must be side-effect-free identifiers; they are evaluated repeatedly.
    /// This statement sequence is used only as a standalone phase here. Rejection
    /// releases the enclosing query's scratch block and returns from its void function.
#define WORLD_COLLISION_REJECT_OUTSIDE_VIEW_BOUNDARY_QUAD(bodyArg, boundaryArg, scratchArg)                                                                                                                               \
    gte_ldv0(&(boundaryArg)->vertices[0]);                                                                                                                                                                                \
    gte_rtv0();                                                                                                                                                                                                           \
    gte_stlvnl(&(scratchArg)->corners[0]);                                                                                                                                                                                \
    (scratchArg)->corners[0].vx += (scratchArg)->origin.vx;                                                                                                                                                               \
    (scratchArg)->corners[0].vy += (scratchArg)->origin.vy;                                                                                                                                                               \
    (scratchArg)->corners[0].vz += (scratchArg)->origin.vz;                                                                                                                                                               \
                                                                                                                                                                                                                          \
    gte_ldv0(&(boundaryArg)->normal);                                                                                                                                                                                     \
    gte_rtv0();                                                                                                                                                                                                           \
    gte_stlvnl(&(scratchArg)->faceNormal);                                                                                                                                                                                \
                                                                                                                                                                                                                          \
    planeOffset = ((scratchArg)->faceNormal.vx * (scratchArg)->corners[0].vx + (scratchArg)->faceNormal.vy * (scratchArg)->corners[0].vy +                                                                                \
                   (scratchArg)->faceNormal.vz * (scratchArg)->corners[0].vz) >>                                                                                                                                          \
                  WORLD_COLLISION_DIRECTION_FRACTION_BITS;                                                                                                                                                                \
    planeDistance = (((scratchArg)->faceNormal.vx * (scratchArg)->sphereCenter.vx + (scratchArg)->faceNormal.vy * (scratchArg)->sphereCenter.vy +                                                                         \
                      (scratchArg)->faceNormal.vz * (scratchArg)->sphereCenter.vz) >>                                                                                                                                     \
                     WORLD_COLLISION_DIRECTION_FRACTION_BITS) -                                                                                                                                                           \
                    planeOffset;                                                                                                                                                                                          \
    if (planeDistance >= 0 || planeDistance < -(bodyArg)->radius) {                                                                                                                                                       \
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);                                                                                                                                            \
        return;                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                     \
                                                                                                                                                                                                                          \
    for (edgeIndex = 1; edgeIndex < (s32)ARRAY_SIZE((boundaryArg)->vertices); edgeIndex++) {                                                                                                                              \
        gte_ldv0(&(boundaryArg)->vertices[edgeIndex]);                                                                                                                                                                    \
        gte_rtv0();                                                                                                                                                                                                       \
        gte_stlvnl(&(scratchArg)->corners[edgeIndex]);                                                                                                                                                                    \
        (scratchArg)->corners[edgeIndex].vx += (scratchArg)->origin.vx;                                                                                                                                                   \
        (scratchArg)->corners[edgeIndex].vy += (scratchArg)->origin.vy;                                                                                                                                                   \
        (scratchArg)->corners[edgeIndex].vz += (scratchArg)->origin.vz;                                                                                                                                                   \
    }                                                                                                                                                                                                                     \
                                                                                                                                                                                                                          \
    for (edgeIndex = 1; edgeIndex < (s32)ARRAY_SIZE((boundaryArg)->vertices) + 1; edgeIndex++) {                                                                                                                          \
        edgeEnd                                = &(scratchArg)->corners[(u16)Gp_FaceEdgePairs[edgeIndex].endCornerIndex];                                                                                                 \
        edgeStart                              = &(scratchArg)->corners[(u16)Gp_FaceEdgePairs[edgeIndex].startCornerIndex];                                                                                               \
        (scratchArg)->work.edgeDisplacement.vx = edgeEnd->vx - edgeStart->vx;                                                                                                                                             \
        (scratchArg)->work.edgeDisplacement.vy = edgeEnd->vy - edgeStart->vy;                                                                                                                                             \
        (scratchArg)->work.edgeDisplacement.vz = edgeEnd->vz - edgeStart->vz;                                                                                                                                             \
        gte_ldopv1(&(scratchArg)->faceNormal);                                                                                                                                                                            \
        gte_ldopv2(&(scratchArg)->work.edgeDisplacement);                                                                                                                                                                 \
        gte_op12();                                                                                                                                                                                                       \
        gte_stlvnl(&(scratchArg)->edgePlaneNormal);                                                                                                                                                                       \
        overlapMeasure   = (scratchArg)->edgePlaneNormal.vx * (scratchArg)->sphereCenter.vx + (scratchArg)->edgePlaneNormal.vy * (scratchArg)->sphereCenter.vy;                                                           \
        overlapMeasure  += (scratchArg)->edgePlaneNormal.vz * (scratchArg)->sphereCenter.vz;                                                                                                                              \
        overlapMeasure >>= WORLD_COLLISION_DIRECTION_FRACTION_BITS;                                                                                                                                                       \
        overlapMeasure  -= ((scratchArg)->edgePlaneNormal.vx * edgeEnd->vx + (scratchArg)->edgePlaneNormal.vy * edgeEnd->vy + (scratchArg)->edgePlaneNormal.vz * edgeEnd->vz) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS; \
        if (overlapMeasure >= 0) {                                                                                                                                                                                        \
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);                                                                                                                                        \
            return;                                                                                                                                                                                                       \
        }                                                                                                                                                                                                                 \
    }

    WORLD_COLLISION_REJECT_OUTSIDE_VIEW_BOUNDARY_QUAD(body, boundary, scratch);
#undef WORLD_COLLISION_REJECT_OUTSIDE_VIEW_BOUNDARY_QUAD

    boundary->hit = WORLD_COLLISION_VIEW_BOUNDARY_HIT;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionViewBoundarySphereScratch);
}

s32 worldCollisionTestOccluderSegment(const WorldCollisionOccluder* occluder, const SVECTOR* start, const SVECTOR* end, const VECTOR* direction)
{
    _WorldCollisionOccluderSegmentScratch* scratch;
    VECTOR*                                edgeEnd;
    VECTOR*                                edgeStart;
    s32                                    normalDirectionDot;
    s32                                    planeDistanceAlongSegment;
    s32                                    intersectionEdgeDot;
    s32                                    edgeIndex;
    s16                                    planeOffset;
    s16                                    edgePlaneOffset;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCollisionOccluderSegmentScratch);

    // Transform the occluder quad into the segment's query space.
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&occluder->origin);
    gte_rtv0();
    gte_stlvnl(&scratch->position.origin);
    scratch->position.origin.vx += gGfxViewCoord.workm.t[0];
    scratch->position.origin.vy += gGfxViewCoord.workm.t[1];
    scratch->position.origin.vz += gGfxViewCoord.workm.t[2];

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&occluder->vertices[0]);
    gte_rtv0();
    gte_stlvnl(&scratch->corners[0]);
    scratch->corners[0].vx += scratch->position.origin.vx;
    scratch->corners[0].vy += scratch->position.origin.vy;
    scratch->corners[0].vz += scratch->position.origin.vz;

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&occluder->normal);
    gte_rtv0();
    gte_stlvnl(&scratch->faceNormal);

    // Solve the plane crossing with signed-halfword plane and edge offsets.
    planeOffset = (scratch->faceNormal.vx * scratch->corners[0].vx + scratch->faceNormal.vy * scratch->corners[0].vy +
                   scratch->faceNormal.vz * scratch->corners[0].vz) >>
                  WORLD_COLLISION_DIRECTION_FRACTION_BITS;
    normalDirectionDot = (scratch->faceNormal.vx * direction->vx + scratch->faceNormal.vy * direction->vy + scratch->faceNormal.vz * direction->vz) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS;
    if (normalDirectionDot == 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);
        return 0;
    }
    planeDistanceAlongSegment = ((scratch->faceNormal.vx * start->vx + scratch->faceNormal.vy * start->vy + scratch->faceNormal.vz * start->vz) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS) -
                                planeOffset;
    planeDistanceAlongSegment = -(planeDistanceAlongSegment << WORLD_COLLISION_DIRECTION_FRACTION_BITS) / normalDirectionDot;
    if (planeDistanceAlongSegment == 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);
        return 0;
    }

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    for (edgeIndex = 1; edgeIndex < (s32)ARRAY_SIZE(occluder->vertices); edgeIndex++) {
        gte_ldv0(&occluder->vertices[edgeIndex]);
        gte_rtv0();
        gte_stlvnl(&scratch->corners[edgeIndex]);
        scratch->corners[edgeIndex].vx += scratch->position.origin.vx;
        scratch->corners[edgeIndex].vy += scratch->position.origin.vy;
        scratch->corners[edgeIndex].vz += scratch->position.origin.vz;
    }

    scratch->position.intersection.vx = start->vx + ((direction->vx * planeDistanceAlongSegment) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS);
    scratch->position.intersection.vy = start->vy + ((direction->vy * planeDistanceAlongSegment) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS);
    scratch->position.intersection.vz = start->vz + ((direction->vz * planeDistanceAlongSegment) >> WORLD_COLLISION_DIRECTION_FRACTION_BITS);

    // An intersection at either endpoint does not block the segment.
    if ((scratch->position.intersection.vx - start->vx) * (scratch->position.intersection.vx - end->vx) +
            (scratch->position.intersection.vy - start->vy) * (scratch->position.intersection.vy - end->vy) +
            (scratch->position.intersection.vz - start->vz) * (scratch->position.intersection.vz - end->vz) >=
        0) {
        SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);
        return 0;
    }

    /// Rejects an intersection outside the placed occluder's quad edges.
    ///
    /// Requires placed corners, face normal and intersection. Captures `edgeIndex`,
    /// `edgeEnd`, `edgeStart`, the signed-halfword `edgePlaneOffset` and
    /// `intersectionEdgeDot`. Arguments must be side-effect-free identifiers;
    /// they are evaluated repeatedly. This statement sequence is used only as a
    /// standalone phase here. Rejection releases the enclosing query's scratch
    /// block and returns 0 from that function; edge equality is accepted.
#define WORLD_COLLISION_REJECT_OUTSIDE_OCCLUDER_EDGES(occluderArg, scratchArg)                                               \
    for (edgeIndex = 1; edgeIndex < (s32)ARRAY_SIZE((occluderArg)->vertices) + 1; edgeIndex++) {                             \
        edgeEnd                           = &(scratchArg)->corners[(u16)Gp_FaceEdgePairs[edgeIndex].endCornerIndex];         \
        edgeStart                         = &(scratchArg)->corners[(u16)Gp_FaceEdgePairs[edgeIndex].startCornerIndex];       \
        (scratchArg)->edgeDisplacement.vx = edgeEnd->vx - edgeStart->vx;                                                     \
        (scratchArg)->edgeDisplacement.vy = edgeEnd->vy - edgeStart->vy;                                                     \
        (scratchArg)->edgeDisplacement.vz = edgeEnd->vz - edgeStart->vz;                                                     \
        gte_ldopv1(&(scratchArg)->faceNormal);                                                                               \
        gte_ldopv2(&(scratchArg)->edgeDisplacement);                                                                         \
        gte_op12();                                                                                                          \
        gte_stlvnl(&(scratchArg)->edgePlaneNormal);                                                                          \
        edgePlaneOffset = ((scratchArg)->edgePlaneNormal.vx * edgeEnd->vx + (scratchArg)->edgePlaneNormal.vy * edgeEnd->vy + \
                           (scratchArg)->edgePlaneNormal.vz * edgeEnd->vz) >>                                                \
                          WORLD_COLLISION_DIRECTION_FRACTION_BITS;                                                           \
        intersectionEdgeDot = ((scratchArg)->edgePlaneNormal.vx * (scratchArg)->position.intersection.vx +                   \
                               (scratchArg)->edgePlaneNormal.vy * (scratchArg)->position.intersection.vy +                   \
                               (scratchArg)->edgePlaneNormal.vz * (scratchArg)->position.intersection.vz) >>                 \
                              WORLD_COLLISION_DIRECTION_FRACTION_BITS;                                                       \
        if (intersectionEdgeDot - edgePlaneOffset > 0) {                                                                     \
            SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);                                              \
            return 0;                                                                                                        \
        }                                                                                                                    \
    }

    WORLD_COLLISION_REJECT_OUTSIDE_OCCLUDER_EDGES(occluder, scratch);
#undef WORLD_COLLISION_REJECT_OUTSIDE_OCCLUDER_EDGES

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCollisionOccluderSegmentScratch);
    return 1;
}
