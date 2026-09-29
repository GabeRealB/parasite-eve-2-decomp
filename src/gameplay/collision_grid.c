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

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `func_800DEAFC`.
/// `in` is the SVECTOR promoted to VECTOR for `ApplyTransposeMatrixLV`;
/// `out` is that transform; `pos0` / `pos1` are the 16-bit grid-space
/// results passed to `func_800DE2C0`.
typedef struct _GpGridPairScratch {
    /* 0x00 */ VECTOR in;
    /* 0x10 */ VECTOR out;
    /* 0x20 */ VECTOR pos0;
    /* 0x30 */ VECTOR pos1;
} GpGridPairScratch;
STATIC_ASSERT_SIZEOF(GpGridPairScratch, 0x40);

/// 0x28-byte scratch from `G_SCRATCH_HEAD` used by `func_800DE2C0`.
/// `vec` is the XZ endpoint difference, normalised into `nrm`. `cell`
/// holds a grid-cell centre; `d` holds the endpoint extension or the
/// distance from the cell centre to the point or segment being marked.
typedef struct _GpMarkScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR nrm;
    /* 0x18 */ SVECTOR cell;
    /* 0x20 */ SVECTOR d;
} GpMarkScratch;
STATIC_ASSERT_SIZEOF(GpMarkScratch, 0x28);

/// 0x40-byte scratch from `G_SCRATCH_HEAD` used by `func_800DE7CC`.
/// `from` / `to` are the two probe endpoints promoted to VECTOR; `delta`
/// is `from - to`, normalised into `dir` for `func_800DD324`; `hit` is the
/// intersection that function writes back, which becomes the next `from`.
typedef struct _GpRayHitScratch {
    /* 0x00 */ VECTOR  from;
    /* 0x10 */ VECTOR  to;
    /* 0x20 */ VECTOR  delta;
    /* 0x30 */ SVECTOR dir;
    /* 0x38 */ SVECTOR hit;
} GpRayHitScratch;
STATIC_ASSERT_SIZEOF(GpRayHitScratch, 0x40);

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `func_800DEC80`.
/// `local` is `GpObj.ctx.d4rec` as `SVECTOR[2]` plus `GpObj.pos`,
/// rotated by `coord->workm` into `vec` then added to `workm.t`.
/// `vec` is reused as `arg1[0] - arg1[1]` for `VectorNormalS`.
typedef struct _GpNormScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR local;
} GpNormScratch;
STATIC_ASSERT_SIZEOF(GpNormScratch, 0x18);

/// 0x98-byte scratch from `G_SCRATCH_HEAD` used by `func_800DEF80`.
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

/// 0x80-byte scratch from `G_SCRATCH_HEAD` used by `func_800DFCCC`.
/// `origin` holds the transformed quad origin, then the segment-plane hit.
/// `edge` and `cross` hold each edge's separating-plane calculation.
typedef struct _GpFaceHitScratch {
    /* 0x00 */ VECTOR verts[4];
    /* 0x40 */ VECTOR origin;
    /* 0x50 */ VECTOR normal;
    /* 0x60 */ VECTOR edge;
    /* 0x70 */ VECTOR cross;
} GpFaceHitScratch;
STATIC_ASSERT_SIZEOF(GpFaceHitScratch, 0x80);

/// 0x50-byte scratch from `G_SCRATCH_HEAD` used by `func_800DDC2C` and
/// `func_800DE150`. `src[0]` / `src[1]` are the local XZ endpoints of
/// `GpObj.pos` offset by `ctx.dir->dir` (as an SVECTOR) scaled by
/// `radius >> 12` (`func_800DDC2C`), or by the two `SVECTOR`s `ctx.d4rec`
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

/// 0x50-byte scratch from `G_SCRATCH_HEAD` used by `func_800DD940`.
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

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `func_800DDDF8`.
/// `pos` holds the world-space segment from `func_800DEC80`; `ray[0]`
/// is its normalized direction and `ray[1]` receives the intersection
/// from `func_800DD324` before it is copied into a collision record.
typedef struct _GpSegmentHitScratch {
    /* 0x00 */ VECTOR  pos[2];
    /* 0x20 */ SVECTOR ray[2];
} GpSegmentHitScratch;
STATIC_ASSERT_SIZEOF(GpSegmentHitScratch, 0x30);

/// 0xB0-byte scratch from `G_SCRATCH_HEAD` used by `func_800DF6AC`: the
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

GpGridParams* Gp_GridParams;

u8 D_80115450[256];

GpObj3A* D_80115550;

GpObj4C* Gp_Obj4CList;

#include "gameplay/world_collision.h"

#include "world_collision.h"

static __inline__ void Gp_ObjWorldPosInline(GpObj* obj, VECTOR* pos);

static void func_800DDC2C(GpObj* arg0);

static void func_800DE150(GpObj* arg0);

static void func_800DE2C0(VECTOR* arg0, s32 arg1);

static void func_800DEAFC(SVECTOR* arg0, SVECTOR* arg1);

static __inline__ void Gp_ObjWorldPosInline(GpObj* obj, VECTOR* pos)
{
    u8*     h;
    VECTOR* vec;
    h                  = SCRATCH_HEAD(u8);
    vec                = (VECTOR*)(h - 0x30);
    SCRATCH_HEAD(void) = vec;
    gte_SetRotMatrix(&obj->coord->workm);
    gte_ldv0(&obj->pos);
    gte_rtv0();
    gte_stlvnl(vec);
    pos->vx = (obj->coord)->workm.t[0] + ((VECTOR*)(h - 0x30))->vx;
    pos->vy = (obj->coord)->workm.t[1] + vec->vy;
    pos->vz = (obj->coord)->workm.t[2] + vec->vz;
    SCRATCH_POP_BYTES(0x30);
}

void func_800DD940(GpObj* arg0)
{
    u8*                    head;
    GpFloorScratch*        block;
    WorldCollisionContact* slot;
    s32                    i;
    u16                    flags;

    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(void) = head - 0x50;
    block              = (GpFloorScratch*)(head - 0x50);
    for (i = 0; i < Gp_GridParams->field_22; i++) {
        D_80115450[i] = 0;
    }
    func_800DDC2C(arg0);
    func_800E0994(arg0, block->seg, block->ray);
    block->origin.vx = block->seg[0].vx;
    block->origin.vy = block->seg[0].vy;
    block->origin.vz = block->seg[0].vz;
    for (i = 0; i < Gp_GridParams->field_22; i++) {
        if (D_80115450[i] &&
            Gp_GridParams->field_4[Gp_GridParams->field_C[i].normalIndex].vy < -0xDDA &&
            func_800DD324(i, block->seg, block->ray, arg0)) {
            slot  = arg0->ctx.dir->field_8;
            flags = slot->flags;
            if (flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
                if ((u32)(slot->key.value & 0xF) < (u32)Gp_GridParams->field_C[i].surfaceClass) {
                    slot->key.value = Gp_GridParams->field_C[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID_FLOOR;
                }
            } else {
                slot->flags     = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                slot->key.value = Gp_GridParams->field_C[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID_FLOOR;
            }
            slot->point           = block->ray[1];
            slot->response.normal = Gp_GridParams->field_4[Gp_GridParams->field_C[i].normalIndex];
            block->delta.vx       = block->origin.vx - block->ray[1].vx;
            block->delta.vy       = block->origin.vy - block->ray[1].vy;
            block->delta.vz       = block->origin.vz - block->ray[1].vz;
            slot->distance        = SquareRoot0(block->delta.vx * block->delta.vx +
                                                block->delta.vy * block->delta.vy + block->delta.vz * block->delta.vz);
            block->seg[0].vx      = block->ray[1].vx;
            block->seg[0].vy      = block->ray[1].vy;
            block->seg[0].vz      = block->ray[1].vz;
        }
    }
    SCRATCH_POP_BYTES(0x50);
}

static void func_800DDC2C(GpObj* arg0)
{
    s32            i;
    GpEdgeScratch* block;
    SVECTOR*       dir;
    MATRIX*        mat;

    dir              = &arg0->ctx.dir->dir;
    block            = SCRATCH_PUSH(GpEdgeScratch);
    mat              = &block->mat;
    block->src[0].vx = (u16)arg0->pos.vx + ((dir->vx * (u16)arg0->radius) >> 12);
    block->src[0].vy = 0;
    block->src[0].vz = (u16)arg0->pos.vz + ((dir->vz * (u16)arg0->radius) >> 12);
    block->src[1].vx = (u16)arg0->pos.vx + (-(dir->vx * (u16)arg0->radius) >> 12);
    block->src[1].vy = 0;
    block->src[1].vz = (u16)arg0->pos.vz + (-(dir->vz * (u16)arg0->radius) >> 12);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &arg0->coord->workm, mat);
    gte_SetRotMatrix(mat);
    for (i = 0; i < 2; i++) {
        gte_ldv0(&block->src[i]);
        gte_rtv0();
        gte_stlvnl(&block->pos[i]);
        block->pos[i].vx = block->pos[i].vx + block->mat.t[0] + Gp_GridParams->field_14;
        block->pos[i].vy = 0;
        block->pos[i].vz = block->pos[i].vz + block->mat.t[2] + Gp_GridParams->field_18;
    }
    func_800DE2C0(block->pos, 0);
    SCRATCH_POP(GpEdgeScratch);
}

void func_800DDDF8(GpObj* obj)
{
    GpSegmentHitScratch*   block;
    WorldCollisionContact* slot;
    u16                    flags;
    s32                    i;

    block = SCRATCH_PUSH(GpSegmentHitScratch);
    for (i = 0; i < Gp_GridParams->field_22; i++) {
        D_80115450[i] = 0;
    }

    func_800DE150(obj);
    func_800DEC80(obj, block->pos, block->ray, 1);

    for (i = 0; i < Gp_GridParams->field_22; i++) {
        if (D_80115450[i] != 0 && func_800DD324(i, block->pos, block->ray, obj) != 0) {
            slot = obj->ctx.d4rec->recs;
            if (obj->flags & 0x400) {
                if (Gp_RoomParamTables[gGameSession->at4.loc.stage - 1][gGameSession->at4.loc.area - 1]
                                      [Gp_GridParams->field_C[i].surfaceClass]
                                          ->field_1 == 0) {
                    slot->distance        = 0;
                    slot->flags          |= WORLD_COLLISION_CONTACT_OCCUPIED;
                    slot->key.value       = Gp_GridParams->field_C[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                    slot->point           = block->ray[1];
                    slot->response.normal = Gp_GridParams->field_4[Gp_GridParams->field_C[i].normalIndex];
                    block->pos[0].vx      = block->ray[1].vx;
                    block->pos[0].vy      = block->ray[1].vy;
                    block->pos[0].vz      = block->ray[1].vz;
                }
            } else {
                for (;;) {
                    flags = slot->flags;
                    if (!(flags & WORLD_COLLISION_CONTACT_OCCUPIED)) {
                        slot->flags           = flags | WORLD_COLLISION_CONTACT_OCCUPIED;
                        slot->distance        = 0;
                        slot->key.value       = Gp_GridParams->field_C[i].surfaceClass | WORLD_COLLISION_CONTACT_GRID;
                        slot->point           = block->ray[1];
                        slot->response.normal = Gp_GridParams->field_4[Gp_GridParams->field_C[i].normalIndex];
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
    SCRATCH_POP(GpSegmentHitScratch);
}

static void func_800DE150(GpObj* arg0)
{
    s32            i;
    u8*            head;
    GpEdgeScratch* block;
    SVECTOR*       src;
    GfxCoord*      coord;
    MATRIX*        mat;

    coord              = arg0->coord;
    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = head - 0x50;
    block              = (GpEdgeScratch*)(head - 0x50);
    mat                = (MATRIX*)(head - 0x20);
    src                = (SVECTOR*)arg0->ctx.d4rec;
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, mat);
    gte_SetRotMatrix(mat);
    for (i = 0; i < 2; i++) {
        block->src[i].vx = (u16)src[i].vx + (u16)arg0->pos.vx;
        block->src[i].vy = 0;
        block->src[i].vz = (u16)src[i].vz + (u16)arg0->pos.vz;
        gte_ldv0(&block->src[i]);
        gte_rtv0();
        gte_stlvnl(&block->pos[i]);
        block->pos[i].vx = block->pos[i].vx + block->mat.t[0] + Gp_GridParams->field_14;
        block->pos[i].vy = 0;
        block->pos[i].vz = block->pos[i].vz + block->mat.t[2] + Gp_GridParams->field_18;
    }
    func_800DE2C0(block->pos, 1);
    SCRATCH_POP_BYTES(0x50);
}

static void func_800DE2C0(VECTOR* arg0, s32 arg1)
{
    u8*            head;
    GpMarkScratch* block;
    GpGridParams*  p;
    GpGridParams*  p2;
    s32            thresh2;
    u32            cellSize;
    s32            half;
    s32            range;
    s32            thresh;
    s32            i;
    s32            j;
    s32            dot;
    s32            proj;
    s32            vz0;
    s32            vz1;
    s16*           ids;
    s16            id;

    head          = SCRATCH_HEAD(u8);
    cellSize      = Gp_GridParams->field_20;
    block         = (GpMarkScratch*)(SCRATCH_HEAD(void) = head - 0x28);
    block->vec.vx = arg0[0].vx - arg0[1].vx;
    block->vec.vy = 0;
    vz0           = arg0[0].vz;
    vz1           = arg0[1].vz;
    block->vec.vz = vz0 - vz1;
    half          = cellSize >> 1;
    range         = ((half * 0xB5) >> 7) + 1;
    VectorNormalS(&block->vec, &block->nrm);

    if ((block->nrm.vx == 0) && (block->nrm.vz == 0)) {
        for (i = 0; i < Gp_GridParams->field_1C; i++) {
            thresh = range * range;
            for (j = 0; j < Gp_GridParams->field_1E; j++) {
                p              = Gp_GridParams;
                block->cell.vx = i * p->field_20 + (p->field_20 >> 1);
                block->cell.vz = j * p->field_20 + (p->field_20 >> 1);
                block->d.vx    = (u16)block->cell.vx - (u16)arg0[0].vx;
                block->d.vz    = (u16)block->cell.vz - (u16)arg0[0].vz;
                if ((block->d.vx * block->d.vx) + (block->d.vz * block->d.vz) < thresh) {
                    ids = p->field_10[i * p->field_1E + j];
                    if (ids != NULL) {
                        while (*ids != -1) {
                            id             = *ids;
                            D_80115450[id] = 1;
                            ids++;
                        }
                    }
                }
            }
        }
    } else {
        block->d.vx = (block->nrm.vx * range) >> 12;
        block->d.vz = (block->nrm.vz * range) >> 12;
        arg0[0].vx += block->d.vx;
        arg0[0].vz += block->d.vz;
        arg0[1].vx -= block->d.vx;
        arg0[1].vz -= block->d.vz;
        for (i = 0; i < Gp_GridParams->field_1C; i++) {
            thresh2 = range * range;
            for (j = 0; j < Gp_GridParams->field_1E; j++) {
                p2             = Gp_GridParams;
                block->cell.vx = i * p2->field_20 + (p2->field_20 >> 1);
                block->cell.vz = j * p2->field_20 + (p2->field_20 >> 1);
                dot            = ((block->cell.vx - arg0[0].vx) * block->nrm.vx) + ((block->cell.vz - arg0[0].vz) * block->nrm.vz);
                if (dot <= 0) {
                    proj = (((block->cell.vx - arg0[1].vx) * block->nrm.vx) + ((block->cell.vz - arg0[1].vz) * block->nrm.vz)) >> 12;
                    if (proj > 0) {
                        block->d.vx = ((u16)arg0[1].vx + ((block->nrm.vx * proj) >> 12)) - (u16)block->cell.vx;
                        block->d.vz = ((u16)arg0[1].vz + ((block->nrm.vz * proj) >> 12)) - (u16)block->cell.vz;
                        if ((block->d.vx * block->d.vx) + (block->d.vz * block->d.vz) < thresh2) {
                            ids = p2->field_10[i * p2->field_1E + j];
                            if (ids != NULL) {
                                while (*ids != -1) {
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

    SCRATCH_POP_BYTES(0x28);
}

s32 func_800DE7CC(SVECTOR* arg0, SVECTOR* arg1, SVECTOR* arg2, SVECTOR* arg3)
{
    GpGridParams*    params;
    s32              ret;
    GpRayHitScratch* block;
    s32              i;

    params = Gp_GridParams;
    ret    = 0;
    if (params == NULL) {
        return ret;
    }

    {
        u8* head;

        head             = SCRATCH_HEAD(u8);
        i                = 0;
        head            -= 0x40;
        SCRATCH_HEAD(u8) = head;
        block            = (GpRayHitScratch*)head;
        if (ret < params->field_22) {
            do {
                D_80115450[i] = 0;
                i++;
            } while (i < Gp_GridParams->field_22);
        }
    }
    func_800DEAFC(arg0, arg1);
    block->from.vx  = arg0->vx;
    block->from.vy  = arg0->vy;
    block->from.vz  = arg0->vz;
    block->to.vx    = arg1->vx;
    block->to.vy    = arg1->vy;
    block->to.vz    = arg1->vz;
    block->delta.vx = block->from.vx - block->to.vx;
    block->delta.vy = block->from.vy - block->to.vy;
    block->delta.vz = block->from.vz - block->to.vz;
    VectorNormalS(&block->delta, &block->dir);
    for (i = 0; i < Gp_GridParams->field_22; i++) {
        if (D_80115450[i] == 0) {
            continue;
        }
        if (Gp_RoomParamTables[gGameSession->at4.loc.stage - 1][gGameSession->at4.loc.area - 1]
                              [Gp_GridParams->field_C[i].surfaceClass]
                                  ->field_1 != 0) {
            continue;
        }
        if (func_800DD324(i, &block->from, &block->dir, 0) == 0) {
            continue;
        }
        if (arg2 != NULL) {
            arg2->vx = block->hit.vx;
            arg2->vy = block->hit.vy;
            arg2->vz = block->hit.vz;
        }
        if (arg3 != NULL) {
            arg3->vx = Gp_GridParams->field_4[Gp_GridParams->field_C[i].normalIndex].vx;
            arg3->vy = Gp_GridParams->field_4[Gp_GridParams->field_C[i].normalIndex].vy;
            arg3->vz = Gp_GridParams->field_4[Gp_GridParams->field_C[i].normalIndex].vz;
        }
        block->from.vx = block->hit.vx;
        block->from.vy = block->hit.vy;
        block->from.vz = block->hit.vz;
        ret            = 1;
    }
    SCRATCH_POP_BYTES(0x40);
    return ret;
}

static void func_800DEAFC(SVECTOR* arg0, SVECTOR* arg1)
{
    u8*                head;
    GpGridPairScratch* block;
    VECTOR*            out;

    head                            = SCRATCH_HEAD(u8);
    block                           = (GpGridPairScratch*)(head - 0x40);
    block->in.vx                    = arg0->vx;
    block->in.vy                    = arg0->vy;
    block->in.vz                    = arg0->vz;
    out                             = (VECTOR*)(head - 0x30);
    SCRATCH_HEAD(GpGridPairScratch) = block;
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &block->in, out);
    {
        GpGridParams* p = Gp_GridParams;

        block->pos0.vx = (s16)(block->out.vx + p->field_14 - p->field_0->coord.t[0]);
        block->pos0.vy = 0;
        block->pos0.vz = (s16)(block->out.vz + p->field_18 - p->field_0->coord.t[2]);
    }
    block->in.vx = arg1->vx;
    block->in.vy = arg1->vy;
    block->in.vz = arg1->vz;
    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &block->in, out);
    {
        GpGridParams* p = Gp_GridParams;

        block->pos1.vx = (s16)(block->out.vx + p->field_14 - p->field_0->coord.t[0]);
        block->pos1.vy = 0;
        block->pos1.vz = (s16)(block->out.vz + p->field_18 - p->field_0->coord.t[2]);
    }
    func_800DE2C0((VECTOR*)(head - 0x20), 0);
    SCRATCH_POP_BYTES(0x40);
}

void func_800DEC80(GpObj* arg0, VECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    GpNormScratch*         block;
    GpActorD4Rec*          rec;
    SVECTOR*               src;
    WorldCollisionContact* slot;
    s32                    flags;
    s32                    i;

    rec   = arg0->ctx.d4rec;
    block = SCRATCH_PUSH(GpNormScratch);
    i     = 0;

    if (arg3 == 0) {
        if (arg0->flags & WORLD_COLLISION_BODY_SINGLE_CONTACT) {
            slot = arg0->ctx.d4rec->recs;
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
        } else if (arg0->flags & 0x400) {
            slot = arg0->ctx.d4rec->recs;
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
    } else if (arg0->flags & 0x400) {
        slot = arg0->ctx.d4rec->recs;
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
        src             = &rec->end0 + i; // end0 and end1 are adjacent
        block->local.vx = src->vx + arg0->pos.vx;
        block->local.vy = src->vy + arg0->pos.vy;
        block->local.vz = src->vz + arg0->pos.vz;
        gte_ldv0(&block->local);
        gte_rtv0();
        gte_stlvnl(&block->vec);
        arg1[i].vx = block->vec.vx + (arg0->coord)->workm.t[0];
        arg1[i].vy = block->vec.vy + (arg0->coord)->workm.t[1];
        arg1[i].vz = block->vec.vz + (arg0->coord)->workm.t[2];
    }

    block->vec.vx = arg1[0].vx - arg1[1].vx;
    block->vec.vy = arg1[0].vy - arg1[1].vy;
    block->vec.vz = arg1[0].vz - arg1[1].vz;
    VectorNormalS(&block->vec, arg2);

    SCRATCH_POP(GpNormScratch);
}

void func_800DEF80(GpObj* node, GpObj4C* other)
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

    SCRATCH_PUSH(GpQuadHitScratch);
    block = SCRATCH_HEAD(GpQuadHitScratch);
    Gp_ObjWorldPosInline(node, &block->nodePos);
    gte_SetRotMatrix(&other->field_8->workm);
    gte_ldv0(&other->field_C);
    gte_rtv0();
    gte_stlvnl(&block->world);
    block->world.vx += other->field_8->workm.t[0];
    block->world.vy += other->field_8->workm.t[1];
    block->world.vz += other->field_8->workm.t[2];

    block->delta.vx = block->world.vx - block->nodePos.vx;
    block->delta.vy = block->world.vy - block->nodePos.vy;
    block->delta.vz = block->world.vz - block->nodePos.vz;
    distSq          = block->delta.vx * block->delta.vx + block->delta.vy * block->delta.vy +
             block->delta.vz * block->delta.vz;
    tmp = other->field_44 + node->radius;
    if (tmp * tmp < distSq) {
        SCRATCH_POP(GpQuadHitScratch);
        return;
    }

    kind = other->field_4A & 7;
    if (kind == 2) {
        GfxCoord* c;
        s32       m0, m1, m2, a;

        c    = node->coord;
        a    = other->field_3C.vx;
        m0   = a * c->coord.m[0][2];
        a    = other->field_3C.vy;
        m1   = a * c->coord.m[1][2];
        a    = other->field_3C.vz;
        m2   = a * c->coord.m[2][2];
        dot  = m0 + m1;
        dot += m2;
        if (dot > -0xC00000) {
            SCRATCH_POP(GpQuadHitScratch);
            return;
        }
    } else if (kind == 4) {
        if (distSq <= 0x3D08F) {
            other->field_4B = 1;
            SCRATCH_POP(GpQuadHitScratch);
            return;
        }
        block->local.vx = other->field_C.vx;
        block->local.vy = node->coord->coord.t[1] + node->pos.vy;
        block->local.vz = other->field_C.vz;
        gte_SetRotMatrix(&other->field_8->workm);
        gte_ldv0(&block->local);
        gte_rtv0();
        gte_stlvnl(&block->delta);
        block->delta.vx = block->nodePos.vx - (block->delta.vx + other->field_8->workm.t[0]);
        block->delta.vy = block->nodePos.vy - (block->delta.vy + other->field_8->workm.t[1]);
        block->delta.vz = block->nodePos.vz - (block->delta.vz + other->field_8->workm.t[2]);
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
        if (dot > -0xC00000) {
            SCRATCH_POP(GpQuadHitScratch);
            return;
        }
    }

    gte_ldv0(&other->field_14[0]);
    gte_rtv0();
    gte_stlvnl(&block->verts[0]);
    block->verts[0].vx += block->world.vx;
    block->verts[0].vy += block->world.vy;
    block->verts[0].vz += block->world.vz;

    gte_ldv0(&other->field_34);
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
        SCRATCH_POP(GpQuadHitScratch);
        return;
    }

    for (i = 1; i < 4; i++) {
        gte_ldv0(&other->field_14[i]);
        gte_rtv0();
        gte_stlvnl(&block->verts[i]);
        block->verts[i].vx += block->world.vx;
        block->verts[i].vy += block->world.vy;
        block->verts[i].vz += block->world.vz;
    }

    for (i = 1; i < 5; i++) {
        va              = &block->verts[(u16)Gp_FaceEdgePairs[i].field_0];
        vb              = &block->verts[(u16)Gp_FaceEdgePairs[i].field_2];
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
            SCRATCH_POP(GpQuadHitScratch);
            return;
        }
    }

    other->field_4B = 1;
    SCRATCH_POP(GpQuadHitScratch);
}

void func_800DF6AC(GpObj* node, GpObj4C* other, VECTOR3* from)
{
    _GpQuadDirScratch* block;
    s32                dist;
    s32                tmp;
    s32                i;
    VECTOR *           va, *vb;
    s16                faceDot;

    SCRATCH_PUSH(_GpQuadDirScratch);
    block         = SCRATCH_HEAD(_GpQuadDirScratch);
    block->dir.vx = node->coord->coord.t[0] - from->vx;
    block->dir.vy = node->coord->coord.t[1] - from->vy;
    block->dir.vz = node->coord->coord.t[2] - from->vz;
    SquareRoot0(block->dir.vx * block->dir.vx + block->dir.vy * block->dir.vy + block->dir.vz * block->dir.vz);
    VectorNormal(&block->dir, &block->dir);
    if (other->field_34.vx * block->dir.vx + other->field_34.vy * block->dir.vy + other->field_34.vz * block->dir.vz >=
        0) {
        SCRATCH_POP(_GpQuadDirScratch);
        return;
    }

    Gp_ObjWorldPosInline(node, &block->quad.nodePos);
    gte_SetRotMatrix(&other->field_8->workm);
    gte_ldv0(&other->field_C);
    gte_rtv0();
    gte_stlvnl(&block->quad.world);
    block->quad.world.vx += other->field_8->workm.t[0];
    block->quad.world.vy += other->field_8->workm.t[1];
    block->quad.world.vz += other->field_8->workm.t[2];

    block->quad.delta.vx = block->quad.world.vx - block->quad.nodePos.vx;
    block->quad.delta.vy = block->quad.world.vy - block->quad.nodePos.vy;
    block->quad.delta.vz = block->quad.world.vz - block->quad.nodePos.vz;
    tmp                  = other->field_44 + node->radius;
    if (tmp * tmp < block->quad.delta.vx * block->quad.delta.vx + block->quad.delta.vy * block->quad.delta.vy +
                        block->quad.delta.vz * block->quad.delta.vz) {
        SCRATCH_POP(_GpQuadDirScratch);
        return;
    }

    gte_ldv0(&other->field_14[0]);
    gte_rtv0();
    gte_stlvnl(&block->quad.verts[0]);
    block->quad.verts[0].vx += block->quad.world.vx;
    block->quad.verts[0].vy += block->quad.world.vy;
    block->quad.verts[0].vz += block->quad.world.vz;

    gte_ldv0(&other->field_34);
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
        SCRATCH_POP(_GpQuadDirScratch);
        return;
    }

    for (i = 1; i < 4; i++) {
        gte_ldv0(&other->field_14[i]);
        gte_rtv0();
        gte_stlvnl(&block->quad.verts[i]);
        block->quad.verts[i].vx += block->quad.world.vx;
        block->quad.verts[i].vy += block->quad.world.vy;
        block->quad.verts[i].vz += block->quad.world.vz;
    }

    for (i = 1; i < 5; i++) {
        va                   = &block->quad.verts[(u16)Gp_FaceEdgePairs[i].field_0];
        vb                   = &block->quad.verts[(u16)Gp_FaceEdgePairs[i].field_2];
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
            SCRATCH_POP(_GpQuadDirScratch);
            return;
        }
    }

    other->field_4B = 1;
    SCRATCH_POP(_GpQuadDirScratch);
}

s32 func_800DFCCC(GpObj3A* arg0, SVECTOR* arg1, SVECTOR* arg2, VECTOR* arg3)
{
    GpFaceHitScratch* block;
    VECTOR*           va;
    VECTOR*           vb;
    s32               dirDot;
    s32               t;
    s32               hitDot;
    s32               i;
    s16               planeDot;
    s16               edgeDot;

    block = SCRATCH_PUSH(GpFaceHitScratch);

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&arg0->origin);
    gte_rtv0();
    gte_stlvnl(&block->origin);
    block->origin.vx += gGfxViewCoord.workm.t[0];
    block->origin.vy += gGfxViewCoord.workm.t[1];
    block->origin.vz += gGfxViewCoord.workm.t[2];

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&arg0->verts[0]);
    gte_rtv0();
    gte_stlvnl(&block->verts[0]);
    block->verts[0].vx += block->origin.vx;
    block->verts[0].vy += block->origin.vy;
    block->verts[0].vz += block->origin.vz;

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&arg0->normal);
    gte_rtv0();
    gte_stlvnl(&block->normal);

    planeDot = (block->normal.vx * block->verts[0].vx + block->normal.vy * block->verts[0].vy +
                block->normal.vz * block->verts[0].vz) >>
               12;
    dirDot = (block->normal.vx * arg3->vx + block->normal.vy * arg3->vy + block->normal.vz * arg3->vz) >> 12;
    if (dirDot == 0) {
        SCRATCH_POP(GpFaceHitScratch);
        return 0;
    }
    t = ((block->normal.vx * arg1->vx + block->normal.vy * arg1->vy + block->normal.vz * arg1->vz) >> 12) - planeDot;
    t = -(t << 12) / dirDot;
    if (t == 0) {
        SCRATCH_POP(GpFaceHitScratch);
        return 0;
    }

    gte_SetRotMatrix(&gGfxViewCoord.workm);
    for (i = 1; i < 4; i++) {
        gte_ldv0(&arg0->verts[i]);
        gte_rtv0();
        gte_stlvnl(&block->verts[i]);
        block->verts[i].vx += block->origin.vx;
        block->verts[i].vy += block->origin.vy;
        block->verts[i].vz += block->origin.vz;
    }

    block->origin.vx = arg1->vx + ((arg3->vx * t) >> 12);
    block->origin.vy = arg1->vy + ((arg3->vy * t) >> 12);
    block->origin.vz = arg1->vz + ((arg3->vz * t) >> 12);

    if ((block->origin.vx - arg1->vx) * (block->origin.vx - arg2->vx) +
            (block->origin.vy - arg1->vy) * (block->origin.vy - arg2->vy) +
            (block->origin.vz - arg1->vz) * (block->origin.vz - arg2->vz) >=
        0) {
        SCRATCH_POP(GpFaceHitScratch);
        return 0;
    }

    for (i = 1; i < 5; i++) {
        va             = &block->verts[(u16)Gp_FaceEdgePairs[i].field_0];
        vb             = &block->verts[(u16)Gp_FaceEdgePairs[i].field_2];
        block->edge.vx = va->vx - vb->vx;
        block->edge.vy = va->vy - vb->vy;
        block->edge.vz = va->vz - vb->vz;
        gte_ldopv1(&block->normal);
        gte_ldopv2(&block->edge);
        gte_op12();
        gte_stlvnl(&block->cross);
        edgeDot = (block->cross.vx * va->vx + block->cross.vy * va->vy + block->cross.vz * va->vz) >> 12;
        hitDot  = (block->cross.vx * block->origin.vx + block->cross.vy * block->origin.vy +
                  block->cross.vz * block->origin.vz) >>
                 12;
        if (hitDot - edgeDot > 0) {
            SCRATCH_POP(GpFaceHitScratch);
            return 0;
        }
    }
    SCRATCH_POP(GpFaceHitScratch);
    return 1;
}
