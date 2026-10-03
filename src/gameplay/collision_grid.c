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

/// 0x98-byte scratch from the scratch stack used by `func_800DEF80`.
/// Transformed quad corners and normal are tested against `nodePos`;
/// `delta` and `cross` hold each edge's separating-plane calculation.
typedef struct _GpQuadHitScratch {
    /* 0x00 */ VECTOR  verts[4];
    /* 0x40 */ VECTOR  world;
    /* 0x50 */ VECTOR  normal;
    /* 0x60 */ VECTOR  delta;
    /* 0x70 */ VECTOR  cross;
    /* 0x80 */ VECTOR  nodePos;
    /* 0x90 */ SVECTOR local;
} GpQuadHitScratch;
STATIC_ASSERT_SIZEOF(GpQuadHitScratch, 0x98);

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

/// 0x50-byte scratch from the scratch stack used by `func_800DDC2C` and
/// `func_800DE150`. `src[0]` / `src[1]` are the local XZ endpoints of
/// `WorldCollisionBody.pos` offset by `context.motion->motionDirection` scaled by
/// `radius >> 12` (`func_800DDC2C`), or by the two `SVECTOR`s `context.capsule`
/// leads with (`func_800DE150`, which passes 1 to `func_800DE2C0`). `mat`
/// is `gGfxViewCoord.workm * coord->workm`. `pos` holds the rotated endpoints
/// plus `mat.t[0]/t[2]` and `Gp_GridParams` grid offsets, then passed to
/// `func_800DE2C0`.
typedef struct _GpEdgeScratch {
    /* 0x00 */ VECTOR  pos[2];
    /* 0x20 */ SVECTOR src[2];
    /* 0x30 */ MATRIX  mat;
} GpEdgeScratch;
STATIC_ASSERT_SIZEOF(GpEdgeScratch, 0x50);

/// 0x50-byte scratch from the scratch stack used by `func_800DD940`.
/// `seg` holds the object's vertical world-space segment, `origin` saves its
/// first endpoint, and `ray` holds the direction and the latest intersection.
/// `delta` measures the displacement from `origin` to that intersection.
typedef struct _GpFloorScratch {
    /* 0x00 */ VECTOR  seg[2];
    /* 0x20 */ VECTOR  origin;
    /* 0x30 */ VECTOR  delta;
    /* 0x40 */ SVECTOR ray[2];
} GpFloorScratch;
STATIC_ASSERT_SIZEOF(GpFloorScratch, 0x50);

/// 0x30-byte scratch from the scratch stack used by `func_800DDDF8`.
/// `pos` holds the world-space segment from `func_800DEC80`; `ray[0]`
/// is its normalized direction and `ray[1]` receives the intersection
/// from `func_800DD324` before it is copied into a collision record.
typedef struct _GpSegmentHitScratch {
    /* 0x00 */ VECTOR  pos[2];
    /* 0x20 */ SVECTOR ray[2];
} GpSegmentHitScratch;
STATIC_ASSERT_SIZEOF(GpSegmentHitScratch, 0x30);

/// 0xB0-byte scratch from the scratch stack used by `func_800DF6AC`: the
/// quad-test block `func_800DEF80` works in, followed by `dir`, the normalised
/// offset of the object's origin from the point the caller passes. The test
/// goes no further unless `dir` points against the quad's normal.
typedef struct {
    GpQuadHitScratch quad;
    u8               unk98[8];
    VECTOR           dir;
} _GpQuadDirScratch;
STATIC_ASSERT_SIZEOF(_GpQuadDirScratch, 0xB0);

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
    u8*                    head;
    GpFloorScratch*        block;
    WorldCollisionContact* slot;
    s32                    i;
    u16                    flags;

    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - 0x50;
    block                      = (GpFloorScratch*)(head - 0x50);
    for (i = 0; i < Gp_GridParams->faceCount; i++) {
        D_80115450[i] = 0;
    }
    func_800DDC2C(arg0);
    func_800E0994(arg0, block->seg, block->ray);
    block->origin.vx = block->seg[0].vx;
    block->origin.vy = block->seg[0].vy;
    block->origin.vz = block->seg[0].vz;
    for (i = 0; i < Gp_GridParams->faceCount; i++) {
        if (D_80115450[i] &&
            Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex].vy < -0xDDA &&
            func_800DD324(i, block->seg, block->ray, arg0)) {
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
            slot->point              = block->ray[1];
            slot->response.direction = Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex];
            block->delta.vx          = block->origin.vx - block->ray[1].vx;
            block->delta.vy          = block->origin.vy - block->ray[1].vy;
            block->delta.vz          = block->origin.vz - block->ray[1].vz;
            slot->distance           = SquareRoot0(block->delta.vx * block->delta.vx +
                                                   block->delta.vy * block->delta.vy + block->delta.vz * block->delta.vz);
            block->seg[0].vx         = block->ray[1].vx;
            block->seg[0].vy         = block->ray[1].vy;
            block->seg[0].vz         = block->ray[1].vz;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

static void func_800DDC2C(WorldCollisionBody* arg0)
{
    s32            i;
    GpEdgeScratch* block;
    SVECTOR*       motionDirection;
    MATRIX*        mat;

    motionDirection  = &arg0->context.motion->motionDirection;
    block            = SCRATCH_STACK_RESERVE_BLOCK(GpEdgeScratch);
    mat              = &block->mat;
    block->src[0].vx = (u16)arg0->pos.vx + ((motionDirection->vx * arg0->radius) >> 12);
    block->src[0].vy = 0;
    block->src[0].vz = (u16)arg0->pos.vz + ((motionDirection->vz * arg0->radius) >> 12);
    block->src[1].vx = (u16)arg0->pos.vx + (-(motionDirection->vx * arg0->radius) >> 12);
    block->src[1].vy = 0;
    block->src[1].vz = (u16)arg0->pos.vz + (-(motionDirection->vz * arg0->radius) >> 12);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &arg0->coord->workm, mat);
    gte_SetRotMatrix(mat);
    for (i = 0; i < 2; i++) {
        gte_ldv0(&block->src[i]);
        gte_rtv0();
        gte_stlvnl(&block->pos[i]);
        block->pos[i].vx = block->pos[i].vx + block->mat.t[0] + Gp_GridParams->xBias;
        block->pos[i].vy = 0;
        block->pos[i].vz = block->pos[i].vz + block->mat.t[2] + Gp_GridParams->zBias;
    }
    func_800DE2C0(block->pos, 0);
    SCRATCH_STACK_RELEASE_BLOCK(GpEdgeScratch);
}

void func_800DDDF8(WorldCollisionBody* obj)
{
    GpSegmentHitScratch*   block;
    WorldCollisionContact* slot;
    u16                    flags;
    s32                    i;

    block = SCRATCH_STACK_RESERVE_BLOCK(GpSegmentHitScratch);
    for (i = 0; i < Gp_GridParams->faceCount; i++) {
        D_80115450[i] = 0;
    }

    func_800DE150(obj);
    func_800DEC80(obj, block->pos, block->ray, 1);

    for (i = 0; i < Gp_GridParams->faceCount; i++) {
        if (D_80115450[i] != 0 && func_800DD324(i, block->pos, block->ray, obj) != 0) {
            slot = obj->context.capsule->contacts;
            if (obj->flags & WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT) {
                if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1]
                                      [Gp_GridParams->faces[i].surfaceClass]
                                          ->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
                    slot->distance           = 0;
                    slot->flags             |= WORLD_COLLISION_CONTACT_OCCUPIED;
                    slot->key.value          = Gp_GridParams->faces[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                    slot->point              = block->ray[1];
                    slot->response.direction = Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex];
                    block->pos[0].vx         = block->ray[1].vx;
                    block->pos[0].vy         = block->ray[1].vy;
                    block->pos[0].vz         = block->ray[1].vz;
                }
            } else {
                for (;;) {
                    flags = slot->flags;
                    if (!(flags & WORLD_COLLISION_CONTACT_OCCUPIED)) {
                        slot->flags              = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                        slot->distance           = 0;
                        slot->key.value          = Gp_GridParams->faces[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                        slot->point              = block->ray[1];
                        slot->response.direction = Gp_GridParams->normals[Gp_GridParams->faces[i].normalIndex];
                        if (slot->flags & WORLD_COLLISION_CONTACT_LAST) {
                            void** head = SCRATCH_HEAD_ADDR;

                            SCRATCH_POP_BYTES_AT(head, sizeof(GpSegmentHitScratch));
                            return;
                        }
                        break;
                    }
                    if (flags == (WORLD_COLLISION_CONTACT_OCCUPIED | WORLD_COLLISION_CONTACT_LAST)) {
                        void** head = SCRATCH_HEAD_ADDR;

                        SCRATCH_POP_BYTES_AT(head, sizeof(GpSegmentHitScratch));
                        return;
                    }
                    slot++;
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpSegmentHitScratch);
}

static void func_800DE150(WorldCollisionBody* arg0)
{
    s32            i;
    u8*            head;
    GpEdgeScratch* block;
    SVECTOR*       src;
    GfxCoord*      coord;
    MATRIX*        mat;

    coord                      = arg0->coord;
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = head - 0x50;
    block                      = (GpEdgeScratch*)(head - 0x50);
    mat                        = (MATRIX*)(head - 0x20);
    src                        = arg0->context.capsule->ends;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, mat);
    gte_SetRotMatrix(mat);
    for (i = 0; i < 2; i++) {
        block->src[i].vx = (u16)src[i].vx + (u16)arg0->pos.vx;
        block->src[i].vy = 0;
        block->src[i].vz = (u16)src[i].vz + (u16)arg0->pos.vz;
        gte_ldv0(&block->src[i]);
        gte_rtv0();
        gte_stlvnl(&block->pos[i]);
        block->pos[i].vx = block->pos[i].vx + block->mat.t[0] + Gp_GridParams->xBias;
        block->pos[i].vy = 0;
        block->pos[i].vz = block->pos[i].vz + block->mat.t[2] + Gp_GridParams->zBias;
    }
    func_800DE2C0(block->pos, 1);
    SCRATCH_STACK_RELEASE_BYTES(0x50);
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
        if (func_800DD324(i, scratch->endpoints, scratch->ray, 0) == 0) {
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
    GpQuadHitScratch* block;
    s32               distSq;
    s32               kind;
    s32               dot;
    s32               dist;
    s32               tmp;
    s32               i;
    VECTOR *          va, *vb;
    s16               faceDot;

    SCRATCH_STACK_RESERVE_BLOCK(GpQuadHitScratch);
    block = SCRATCH_STACK_CURSOR(GpQuadHitScratch);
    Gp_ObjWorldPosInline(node, &block->nodePos);
    gte_SetRotMatrix(&other->coord->workm);
    gte_ldv0(&other->origin);
    gte_rtv0();
    gte_stlvnl(&block->world);
    block->world.vx += other->coord->workm.t[0];
    block->world.vy += other->coord->workm.t[1];
    block->world.vz += other->coord->workm.t[2];

    block->delta.vx = block->world.vx - block->nodePos.vx;
    block->delta.vy = block->world.vy - block->nodePos.vy;
    block->delta.vz = block->world.vz - block->nodePos.vz;
    distSq          = block->delta.vx * block->delta.vx + block->delta.vy * block->delta.vy +
             block->delta.vz * block->delta.vz;
    tmp = other->radius + node->radius;
    if (tmp * tmp < distSq) {
        SCRATCH_STACK_RELEASE_BLOCK(GpQuadHitScratch);
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
            SCRATCH_STACK_RELEASE_BLOCK(GpQuadHitScratch);
            return;
        }
    } else if (kind == WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD) {
        if (distSq <= WORLD_COLLISION_TRIGGER_NEAR_DISTANCE_SQUARED_MAX) {
            other->hit = 1;
            SCRATCH_STACK_RELEASE_BLOCK(GpQuadHitScratch);
            return;
        }
        block->local.vx = other->origin.vx;
        block->local.vy = node->coord->coord.t[1] + node->pos.vy;
        block->local.vz = other->origin.vz;
        gte_SetRotMatrix(&other->coord->workm);
        gte_ldv0(&block->local);
        gte_rtv0();
        gte_stlvnl(&block->delta);
        block->delta.vx = block->nodePos.vx - (block->delta.vx + other->coord->workm.t[0]);
        block->delta.vy = block->nodePos.vy - (block->delta.vy + other->coord->workm.t[1]);
        block->delta.vz = block->nodePos.vz - (block->delta.vz + other->coord->workm.t[2]);
        VectorNormal(&block->delta, &block->delta);
        {
            GfxCoord* c;
            s32       n0, n1, n2;

            c    = node->coord;
            n0   = block->delta.vx * c->workm.m[0][2];
            n1   = block->delta.vy * c->workm.m[1][2];
            n2   = block->delta.vz * c->workm.m[2][2];
            dot  = n0 + n1;
            dot += n2;
        }
        if (dot > WORLD_COLLISION_TRIGGER_FACING_DOT_MAX) {
            SCRATCH_STACK_RELEASE_BLOCK(GpQuadHitScratch);
            return;
        }
    }

    // Test the sphere against the transformed quad's one-sided plane and edges.
    gte_ldv0(&other->vertices[0]);
    gte_rtv0();
    gte_stlvnl(&block->verts[0]);
    block->verts[0].vx += block->world.vx;
    block->verts[0].vy += block->world.vy;
    block->verts[0].vz += block->world.vz;

    gte_ldv0(&other->normal);
    gte_rtv0();
    gte_stlvnl(&block->normal);

    faceDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
               block->normal.vz * block->verts[0].vz) >>
              12;
    dist = ((block->normal.vx * block->nodePos.vx + block->normal.vy * block->nodePos.vy +
             block->normal.vz * block->nodePos.vz) >>
            12) -
           faceDot;
    if (dist >= 0 || dist < -node->radius) {
        SCRATCH_STACK_RELEASE_BLOCK(GpQuadHitScratch);
        return;
    }

    for (i = 1; i < (s32)ARRAY_SIZE(other->vertices); i++) {
        gte_ldv0(&other->vertices[i]);
        gte_rtv0();
        gte_stlvnl(&block->verts[i]);
        block->verts[i].vx += block->world.vx;
        block->verts[i].vy += block->world.vy;
        block->verts[i].vz += block->world.vz;
    }

    for (i = 1; i < (s32)ARRAY_SIZE(other->vertices) + 1; i++) {
        va              = &block->verts[(u16)Gp_FaceEdgePairs[i].endCornerIndex];
        vb              = &block->verts[(u16)Gp_FaceEdgePairs[i].startCornerIndex];
        block->delta.vx = va->vx - vb->vx;
        block->delta.vy = va->vy - vb->vy;
        block->delta.vz = va->vz - vb->vz;
        gte_ldopv1(&block->normal);
        gte_ldopv2(&block->delta);
        gte_op12();
        gte_stlvnl(&block->cross);
        tmp   = block->cross.vx * block->nodePos.vx + block->cross.vy * block->nodePos.vy;
        tmp  += block->cross.vz * block->nodePos.vz;
        tmp >>= 12;
        tmp  -= (block->cross.vx * va->vx + block->cross.vy * va->vy + block->cross.vz * va->vz) >> 12;
        if (tmp >= 0) {
            SCRATCH_STACK_RELEASE_BLOCK(GpQuadHitScratch);
            return;
        }
    }

    other->hit = 1;
    SCRATCH_STACK_RELEASE_BLOCK(GpQuadHitScratch);
}

void func_800DF6AC(WorldCollisionBody* node, WorldCollisionTrigger* other, VECTOR3* from)
{
    _GpQuadDirScratch* block;
    s32                dist;
    s32                tmp;
    s32                i;
    VECTOR *           va, *vb;
    s16                faceDot;

    SCRATCH_STACK_RESERVE_BLOCK(_GpQuadDirScratch);
    block         = SCRATCH_STACK_CURSOR(_GpQuadDirScratch);
    block->dir.vx = node->coord->coord.t[0] - from->vx;
    block->dir.vy = node->coord->coord.t[1] - from->vy;
    block->dir.vz = node->coord->coord.t[2] - from->vz;
    SquareRoot0(block->dir.vx * block->dir.vx + block->dir.vy * block->dir.vy + block->dir.vz * block->dir.vz);
    VectorNormal(&block->dir, &block->dir);
    // A view boundary accepts only movement against its room-space normal.
    if (other->normal.vx * block->dir.vx + other->normal.vy * block->dir.vy + other->normal.vz * block->dir.vz >=
        0) {
        SCRATCH_STACK_RELEASE_BLOCK(_GpQuadDirScratch);
        return;
    }

    Gp_ObjWorldPosInline(node, &block->quad.nodePos);
    gte_SetRotMatrix(&other->coord->workm);
    gte_ldv0(&other->origin);
    gte_rtv0();
    gte_stlvnl(&block->quad.world);
    block->quad.world.vx += other->coord->workm.t[0];
    block->quad.world.vy += other->coord->workm.t[1];
    block->quad.world.vz += other->coord->workm.t[2];

    block->quad.delta.vx = block->quad.world.vx - block->quad.nodePos.vx;
    block->quad.delta.vy = block->quad.world.vy - block->quad.nodePos.vy;
    block->quad.delta.vz = block->quad.world.vz - block->quad.nodePos.vz;
    tmp                  = other->radius + node->radius;
    if (tmp * tmp < block->quad.delta.vx * block->quad.delta.vx + block->quad.delta.vy * block->quad.delta.vy +
                        block->quad.delta.vz * block->quad.delta.vz) {
        SCRATCH_STACK_RELEASE_BLOCK(_GpQuadDirScratch);
        return;
    }

    gte_ldv0(&other->vertices[0]);
    gte_rtv0();
    gte_stlvnl(&block->quad.verts[0]);
    block->quad.verts[0].vx += block->quad.world.vx;
    block->quad.verts[0].vy += block->quad.world.vy;
    block->quad.verts[0].vz += block->quad.world.vz;

    gte_ldv0(&other->normal);
    gte_rtv0();
    gte_stlvnl(&block->quad.normal);

    faceDot = (block->quad.normal.vx * block->quad.verts[0].vx + block->quad.normal.vy * block->quad.verts[0].vy +
               block->quad.normal.vz * block->quad.verts[0].vz) >>
              12;
    dist = ((block->quad.normal.vx * block->quad.nodePos.vx + block->quad.normal.vy * block->quad.nodePos.vy +
             block->quad.normal.vz * block->quad.nodePos.vz) >>
            12) -
           faceDot;
    if (dist >= 0 || dist < -node->radius) {
        SCRATCH_STACK_RELEASE_BLOCK(_GpQuadDirScratch);
        return;
    }

    for (i = 1; i < (s32)ARRAY_SIZE(other->vertices); i++) {
        gte_ldv0(&other->vertices[i]);
        gte_rtv0();
        gte_stlvnl(&block->quad.verts[i]);
        block->quad.verts[i].vx += block->quad.world.vx;
        block->quad.verts[i].vy += block->quad.world.vy;
        block->quad.verts[i].vz += block->quad.world.vz;
    }

    for (i = 1; i < (s32)ARRAY_SIZE(other->vertices) + 1; i++) {
        va                   = &block->quad.verts[(u16)Gp_FaceEdgePairs[i].endCornerIndex];
        vb                   = &block->quad.verts[(u16)Gp_FaceEdgePairs[i].startCornerIndex];
        block->quad.delta.vx = va->vx - vb->vx;
        block->quad.delta.vy = va->vy - vb->vy;
        block->quad.delta.vz = va->vz - vb->vz;
        gte_ldopv1(&block->quad.normal);
        gte_ldopv2(&block->quad.delta);
        gte_op12();
        gte_stlvnl(&block->quad.cross);
        tmp   = block->quad.cross.vx * block->quad.nodePos.vx + block->quad.cross.vy * block->quad.nodePos.vy;
        tmp  += block->quad.cross.vz * block->quad.nodePos.vz;
        tmp >>= 12;
        tmp  -= (block->quad.cross.vx * va->vx + block->quad.cross.vy * va->vy + block->quad.cross.vz * va->vz) >> 12;
        if (tmp >= 0) {
            SCRATCH_STACK_RELEASE_BLOCK(_GpQuadDirScratch);
            return;
        }
    }

    other->hit = 1;
    SCRATCH_STACK_RELEASE_BLOCK(_GpQuadDirScratch);
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
