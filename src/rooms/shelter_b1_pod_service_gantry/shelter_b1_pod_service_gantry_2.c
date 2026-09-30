#include "rooms/shelter_b1_pod_service_gantry.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b1_pod_service_gantry_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_flags.h"
#include "gameplay/effects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Temporary projection and corner offsets for the room's spinning sprite.
typedef struct {
    s32     otz;  // Projected ordering-table depth (SZ3 / 4)
    s32     flag; // GTE projection status; negative rejects the sprite
    s32     dx;   // Rotated horizontal half-extent; the corner stores use its low half
    s32     dy;   // Rotated vertical half-extent; the corner stores use its low half
    SVECTOR vec;  // Low 16 bits of the coordinate's cached translation
    u16     sx;   // Projected screen X, interpreted modulo 65536 by the corner arithmetic
    u16     sy;   // Projected screen Y, interpreted modulo 65536 by the corner arithmetic
} _ShelterB1PodServiceGantrySpinScratch;
STATIC_ASSERT_SIZEOF(_ShelterB1PodServiceGantrySpinScratch, 0x1C);

extern s32 D_801752EC;
extern s8  D_shelter_b1_pod_service_gantry_8018256C[];

static void func_shelter_b1_pod_service_gantry_8017DF70(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b1_pod_service_gantry_8017E400(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b1_pod_service_gantry_8017ED3C(GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle);
static void func_shelter_b1_pod_service_gantry_8017F160(GfxCoord* arg0, u16 arg1, s16 arg2);

s32 D_shelter_b1_pod_service_gantry_8018250C[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

GpRoomParamRec D_shelter_b1_pod_service_gantry_80182518[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec* D_shelter_b1_pod_service_gantry_80182520[8] = {
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
    D_shelter_b1_pod_service_gantry_80182518,
};

GpAreaApplyRec D_shelter_b1_pod_service_gantry_80182540[11] = {
    { 4, 17, 22, 0 },
    { 4, 25, 1, 0 },
    { 4, 27, 1, 0 },
    { 4, 28, 1, 0 },
    { 4, 29, 1, 0 },
    { 4, 30, 1, 0 },
    { 4, 32, 1, 0 },
    { 4, 33, 1, 0 },
    { 4, 34, 1, 0 },
    { 4, 35, 22, 0 },
    { 255, 0, 0, 0 },
};

s8 D_shelter_b1_pod_service_gantry_8018256C[8] = { 0 };

/// Per-frame driver of an animated sprite effect, a `Gp_State1C` effect task
/// drawn through `func_shelter_b1_pod_service_gantry_8017DF70` (state 1) or,
/// when the spawn argument is negative,
/// `func_shelter_b1_pod_service_gantry_8017E400` (state 2). The first tick
/// unpacks the spawn argument: the low 12 bits are the sprite size, bits
/// 12-14 the ticks per animation cell (1 when zero) and bits 28-30 the CLUT
/// selector passed to the drawer above the cell index; the spin angle is
/// rolled from `Gp_LcgState`. When the work block arrives without a velocity,
/// bits 24-27 choose how one is rolled (0 leaves the sprite still, 5 takes the
/// block's stored direction) and it is scaled to the speed in bits 16-23
/// (0x40 when zero). Each later tick draws the current cell, moves the
/// coordinate by the velocity, bends the vertical velocity (kind 7 by the tick
/// count / 10, otherwise upwards by a constant) and releases the work block
/// after the drawer's last cell (12 cells in state 1, 10 in state 2). During
/// an event it only draws, and releases once the event state reaches 4.
void func_shelter_b1_pod_service_gantry_8017D8F4(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;
    s32        step;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (task->state < 2) {
            func_shelter_b1_pod_service_gantry_8017DF70(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            func_shelter_b1_pod_service_gantry_8017E400(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.value & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            task->state  = 1;
            task->state  = task->spawnArg1.value < 0 ? 2 : 1;
            work->pos.vx = (task->spawnArg1.value >> 16) & 0x7000;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1.value >> 24) & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case 6:
                        work->move.vy = 0;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            break;
        case 1:
            func_shelter_b1_pod_service_gantry_8017DF70(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b1_pod_service_gantry_8017E400(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

/// Draws a camera-facing sprite at `arg0`'s world position: the point is
/// projected through `GsWSMATRIX` into a zeroed scratch block popped from
/// the scratch stack and, when the GTE flag is non-negative, one
/// semi-transparent `POLY_FT4` (tpage 0x2B) is queued with its corners on two
/// radii at angles `arg3` and `arg3 + 0x400`, of length `arg2 * 47` divided by
/// the depth. The low 12 bits of `arg1` pick a 48x48 cell of a five-column
/// texture grid starting at v 0x70. The bits above them select the CLUT: 0 and
/// 1 pick row 0x10E or 0x10F with the column taken from the cell index, and
/// anything higher the fixed CLUT 0x428F.
static void func_shelter_b1_pod_service_gantry_8017DF70(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              bank;
    u32              idx;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;

    idx      = arg1;
    idx     &= 0xFFF;
    bank     = arg1 >> 12;
    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (GpFxQuadScratch*)(head - 0x1C);
    Mem_Set(block, 0, 0x1C);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block->vec.vy                             = (u16)arg0->workm.t[1];
    block->vec.vz                             = (u16)arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        if (bank >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((bank + 0x10E) << 6) | (idx & 0x3F);
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 + 0x70, u0 + 0x2F, v0 + 0x70, u0, v0 + 0x9F, u0 + 0x2F, v0 + 0x9F);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        idx       = ang + 0x400;
        prim->y3  = block->sy + (u16)block->dy;
        block->dx = (((arg2 * 47) / block->otz) * rsin(idx)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(idx)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

/// Draws a camera-facing sprite at `arg0`'s world position: the point is
/// projected through `GsWSMATRIX` into a zeroed scratch block popped from
/// the scratch stack and, when the GTE flag is non-negative, one
/// semi-transparent `POLY_FT4` (tpage 0x2C) is queued with its corners on two
/// radii at angles `arg3` and `arg3 + 0x400`, of length `arg2 * 47` divided by
/// the depth. The low 12 bits of `arg1` pick a 48x48 cell of a five-column
/// texture grid, and any bit above them selects CLUT 0x428F instead of 0x43D0.
static void func_shelter_b1_pod_service_gantry_8017E400(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              bank;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;

    bank     = arg1 >> 12;
    arg1    &= 0xFFF;
    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (GpFxQuadScratch*)(head - 0x1C);
    Mem_Set(block, 0, 0x1C);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block->vec.vy                             = (u16)arg0->workm.t[1];
    block->vec.vz                             = (u16)arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = bank ? 0x428F : 0x43D0;
        col         = arg1 % 5;
        row         = arg1 / 5;
        ang         = arg3;
        u0          = col * 0x30;
        v0          = row * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        ang2      = ang + 0x400;
        prim->y3  = block->sy + (u16)block->dy;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

/// Per-frame driver of a particle effect, a `Gp_State1C` effect task drawn
/// through `func_shelter_b1_pod_service_gantry_8017ED3C` (state 1) or, when
/// the spawn argument's top nibble is set,
/// `func_shelter_b1_pod_service_gantry_8017F160` (state 2). The first tick
/// takes the size from the argument's low 12 bits, a random spin angle, and
/// the ticks per animation frame from bits 12-15 (1 when zero). Unless the
/// work block already carries a velocity, it picks one by the kind in bits
/// 24-27 (0 none, 1-3 random directions, 5 the block's stored direction),
/// scaled to the speed in bits 16-23 (0x40 when zero). Every later tick
/// refreshes the coordinate, draws, moves it by the velocity with 6 added to
/// the vertical component, and releases the block after animation frame 7.
/// During an event it only draws, releasing the block once the event state
/// reaches 4.
void func_shelter_b1_pod_service_gantry_8017E880(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state < 2) {
                func_shelter_b1_pod_service_gantry_8017ED3C(coord, work->index, work->scale, work->angle);
            } else {
                func_shelter_b1_pod_service_gantry_8017F160(coord, work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1.value & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = task->spawnArg1.signedBytes[3];
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_shelter_b1_pod_service_gantry_8017ED3C(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b1_pod_service_gantry_8017F160(coord, work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning, semi-transparent raw-texture quad around the projected coordinate.
///
/// `coord->workm` must already be current in the space `GsWSMATRIX` projects.
/// `textureColumn` selects a 32-texel column on texture row 0xE0..0xFF;
/// UV stores retain only their low byte. The projected radius is
/// `radiusScale * 31 / otz`, with a nonzero depth required. `spinAngle` uses
/// 0x1000 units per turn. The scratch block lives only during this draw.
static void func_shelter_b1_pod_service_gantry_8017ED3C(GfxCoord* coord, u16 textureColumn, s16 radiusScale, s16 spinAngle)
{
    void**                                          scratch;
    u8*                                             head;
    _ShelterB1PodServiceGantrySpinScratch*          block;
    register _ShelterB1PodServiceGantrySpinScratch* depthBlock asm("s0");
    POLY_FT4*                                       prim;
    s32                                             u0;
    s32                                             u1;
    s32                                             v;
    s32                                             angle;
    s32                                             quarterTurnAngle;

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    block    = (_ShelterB1PodServiceGantrySpinScratch*)(head - sizeof(*block));
    *scratch = block;
    Mem_Set(block, 0, sizeof(*block));
    block->vec.vx = (u16)coord->workm.t[0];
    block->vec.vy = (u16)coord->workm.t[1];
    block->vec.vz = (u16)coord->workm.t[2];
    // Reuse the saved coordinate register after capturing the position.
    depthBlock = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((_ShelterB1PodServiceGantrySpinScratch*)(head - sizeof(*block)))->vec);
    gte_rtps();
    gte_stsxy(&((_ShelterB1PodServiceGantrySpinScratch*)(head - sizeof(*block)))->sx);
    gte_stflg(&((_ShelterB1PodServiceGantrySpinScratch*)(head - sizeof(*block)))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&depthBlock->otz);
        prim           = gGpuPrimCursor;
        angle          = spinAngle;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setShadeTex(prim, 1);
        setSemiTrans(prim, 1);
        prim->tpage = 0x2C;
        prim->clut  = 0x43D3;
        u0          = textureColumn << 5;
        v           = 0xE0;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, v, u1, v, u0, 0xFF, u1, 0xFF);
        block->dx        = (((radiusScale * 31) / ((_ShelterB1PodServiceGantrySpinScratch*)(head - sizeof(*block)))->otz) * rsin(angle)) >> 12;
        block->dy        = (((radiusScale * 31) / ((_ShelterB1PodServiceGantrySpinScratch*)(head - sizeof(*block)))->otz) * rcos(angle)) >> 12;
        prim->x0         = block->sx + (u16)block->dx;
        prim->x3         = block->sx - (u16)block->dx;
        prim->y0         = block->sy - (u16)block->dy;
        quarterTurnAngle = angle + 0x400;
        prim->y3         = block->sy + (u16)block->dy;
        block->dx        = (((radiusScale * 31) / ((_ShelterB1PodServiceGantrySpinScratch*)(head - sizeof(*block)))->otz) * rsin(quarterTurnAngle)) >> 12;
        block->dy        = (((radiusScale * 31) / ((_ShelterB1PodServiceGantrySpinScratch*)(head - sizeof(*block)))->otz) * rcos(quarterTurnAngle)) >> 12;
        prim->x1         = block->sx + (u16)block->dx;
        prim->x2         = block->sx - (u16)block->dx;
        prim->y1         = block->sy - (u16)block->dy;
        prim->y2         = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((_ShelterB1PodServiceGantrySpinScratch*)(head - sizeof(*block)))->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(*block));
}

/// Projects the coordinate's world position through `GsWSMATRIX` into a
/// 0x18-byte scratch block zeroed with `Mem_Set` and, when the GTE flag is
/// non-negative, queues one shade-tex `POLY_FT4` (tpage 0x2B, clut 0x4393)
/// with a 56-texel UV tile picked by `arg1` and an on-screen radius of
/// `arg2 * 55 / otz`.
static void func_shelter_b1_pod_service_gantry_8017F160(GfxCoord* arg0, u16 arg1, s16 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    u32            cell;
    s32            tex;
    s32            v0;
    s32            u1;
    s32            v1;
    s16            xy;

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    block    = (GpRingScratch*)(head - 0x18);
    *scratch = block;
    Mem_Set(block, 0, 0x18);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x4393;
        cell        = arg1;
        tex         = (cell & 3) * 0x38;
        v0          = ((cell & 7) >> 2) * 0x38;
        u1          = tex + 0x37;
        v1          = v0 + 0x37;
        setUV4(prim, tex, v0, u1, v0, tex, v1, u1, v1);
        block->step = (arg2 * 55) / block->otz;
        xy          = block->sx - block->step;
        prim->x0 = prim->x2 = xy;
        xy                  = block->sx + block->step;
        prim->x1 = prim->x3 = xy;
        xy                  = block->sy - block->step - (block->step >> 1);
        prim->y0 = prim->y1 = xy;
        xy                  = block->sy + (block->step >> 1);
        prim->y2 = prim->y3 = xy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Draws a glowing disc at the point (0, -0xC4, 0) in `arg0`'s local frame:
/// the point is rotated by `workm`, offset by its translation and projected
/// through `GsWSMATRIX` into a zeroed scratch block popped from
/// the scratch stack. When the GTE flag is non-negative, four Gouraud
/// `POLY_G4` quarter-wedges of radius `arg2 * 64 / otz` are queued, each lit
/// at the centre vertex and black on the rim. `arg3` packs the centre colour
/// as three 4-bit channels (red in bits 8-11, green 4-7, blue 0-3); a one-bit
/// flicker, taken from the global at 0x801752EC plus the per-slot byte
/// `arg1 & 7` of this room's random table, is shifted left by `arg3`'s top
/// nibble and added to every channel.
void func_shelter_b1_pod_service_gantry_8017F450(GfxCoord* arg0, s32 arg1, s32 arg2, s16 arg3)
{
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    s32            t;
    s32            t2;
    s32            blend;
    s32            color;
    u16            color16;
    u32            c;
    s32            green;
    u8             red;

    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - 0x18;
    block                      = SCRATCH_STACK_CURSOR(GpRingScratch);
    color                      = arg3;
    color16                    = color;
    Mem_Set(block, 0, 0x18);
    ((GpRingScratch*)(head - 0x18))->vec.vx = 0;
    block->vec.vy                           = -0xC4;
    block->vec.vz                           = 0;
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(block);
    gte_rtv0();
    gte_stsv(block);
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)((GpRingScratch*)(head - 0x18))->vec.vx + (u16)arg0->workm.t[0];
    block->vec.vy                           = (u16)block->vec.vy + (u16)arg0->workm.t[1];
    block->vec.vz                           = (u16)block->vec.vz + (u16)arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        arg2        = ((s16)arg2 * 64) / block->otz;
        ang         = 0;
        blend       = (D_801752EC + (u8)D_shelter_b1_pod_service_gantry_8018256C[arg1 & 7]) & 1;
        c           = color16;
        blend     <<= c >> 12;
        red         = blend + ((c >> 4) & 0xF0);
        green       = blend + (c & 0xF0);
        arg3        = blend + ((arg3 & 0xF) << 4);
        block->step = arg2;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, red, green, arg3);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = (u16)block->sx + ((block->step * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = (u16)block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = (u16)block->sx + ((block->step * rsin(t)) >> 12);
            prim->y1 = (u16)block->sy + ((block->step * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = (u16)block->sx;
            prim->y2 = (u16)block->sy;
            prim->x3 = (u16)block->sx + ((block->step * rsin(t2)) >> 12);
            prim->y3 = (u16)block->sy + ((block->step * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

/// Per-frame driver of a rising sprite effect, a `Gp_State1C` effect task.
///
/// On its first frame it seeds the rise speed (`move.vy`, 0x10-0x4F from
/// the LCG, negated when bit 16 of `Task::spawnArg1` is set), a random spin
/// angle (`scale`) and the sprite radius (`angle`, the low 12 bits of
/// `Task::spawnArg1`). While no event runs, every frame moves the coordinate
/// frame along Y by the speed and advances the animation frame (`index`)
/// every fourth tick, releasing the effect after frame 7. During an event of
/// state 1-3 it keeps drawing without moving or animating; state 4 or above
/// releases it. Each draw picks one of six sprite CLUTs at random.
void func_shelter_b1_pod_service_gantry_8017F8C8(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
            return;
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_DrawFxQuad(coord, work->index, work->angle, work->scale | (((Gp_LcgState >> 16) % 6) << 12));
        return;
    }
    work->age++;
    if (task->state == 0) {
        work->move.vx = 0;
        work->move.vz = 0;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->move.vy = ((Gp_LcgState >> 16) & 0x3F) + 0x10;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->scale   = (Gp_LcgState >> 16) & 0xFFF;
        work->angle   = task->spawnArg1.value & 0xFFF;
        if (task->spawnArg1.value & 0x10000) {
            work->move.vy = -work->move.vy;
        }
        task->state = 1;
    }
    coord->coord.t[1]  += work->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (!(work->age & 3)) {
        work->index++;
    }
    if (work->index < 8) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_DrawFxQuad(coord, work->index, work->angle, work->scale | (((Gp_LcgState >> 16) % 6) << 12));
        return;
    }
    Gp_ReleaseState1CMem(work, task);
}

void func_shelter_b1_pod_service_gantry_8017FA7C(Task* arg0)
{
    s32 i;

    if (arg0->state == 0) {
        for (i = 0; i < 8; i++) {
            D_shelter_b1_pod_service_gantry_8018256C[i] = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
        }
        D_80115738  = 0x601BE;
        arg0->state = 1;
    }
}
