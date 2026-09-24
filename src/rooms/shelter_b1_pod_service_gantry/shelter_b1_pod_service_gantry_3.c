#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_pod_service_gantry.h"

#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern void D_shelter_b1_pod_service_gantry_8017FAF4;
extern s32  D_801752EC;
extern s8   D_shelter_b1_pod_service_gantry_8018256C[];
extern u32  Gp_LcgState;

void func_shelter_b1_pod_service_gantry_8017DF70(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b1_pod_service_gantry_8017E400(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b1_pod_service_gantry_8017ED3C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b1_pod_service_gantry_8017F160(GsCOORDINATE2* arg0, u16 arg1, s16 arg2);

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
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            step;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (task->state < 2) {
            func_shelter_b1_pod_service_gantry_8017DF70(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
        } else {
            func_shelter_b1_pod_service_gantry_8017E400(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
        }
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = task->spawnArg1 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 7;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            task->state    = 1;
            task->state    = task->spawnArg1 < 0 ? 2 : 1;
            work->field_18 = (task->spawnArg1 >> 16) & 0x7000;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                switch ((task->spawnArg1 >> 24) & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                    case 6:
                        work->field_10.vy = 0;
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12_real();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            break;
        case 1:
            func_shelter_b1_pod_service_gantry_8017DF70(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += (s16)work->field_10.vx;
                coord->coord.t[1] += (s16)work->field_10.vy;
                coord->coord.t[2] += (s16)work->field_10.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->field_10.vy += (s16)work->field_22 / 10;
                } else {
                    work->field_10.vy -= 2;
                }
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b1_pod_service_gantry_8017E400(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += (s16)work->field_10.vx;
                coord->coord.t[1] += (s16)work->field_10.vy;
                coord->coord.t[2] += (s16)work->field_10.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->field_10.vy += (s16)work->field_22 / 10;
                } else {
                    work->field_10.vy -= 1;
                }
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

/// Draws a camera-facing sprite at `arg0`'s world position: the point is
/// projected through `GsWSMATRIX` into a zeroed scratch block popped from
/// `G_SCRATCH_HEAD` and, when the GTE flag is non-negative, one
/// semi-transparent `POLY_FT4` (tpage 0x2B) is queued with its corners on two
/// radii at angles `arg3` and `arg3 + 0x400`, of length `arg2 * 47` divided by
/// the depth. The low 12 bits of `arg1` pick a 48x48 cell of a five-column
/// texture grid starting at v 0x70. The bits above them select the CLUT: 0 and
/// 1 pick row 0x10E or 0x10F with the column taken from the cell index, and
/// anything higher the fixed CLUT 0x428F.
void func_shelter_b1_pod_service_gantry_8017DF70(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                bank;
    u32                idx;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;

    idx      = arg1;
    idx     &= 0xFFF;
    bank     = arg1 >> 12;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (RoomDraw19Scratch*)(head - 0x1C);
    Mem_Set(block, 0, 0x1C);
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    block->vec.vz                               = *(u16*)&arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
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
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        idx       = ang + 0x400;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        block->dx = (((arg2 * 47) / block->otz) * rsin(idx)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(idx)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

/// Draws a camera-facing sprite at `arg0`'s world position: the point is
/// projected through `GsWSMATRIX` into a zeroed scratch block popped from
/// `G_SCRATCH_HEAD` and, when the GTE flag is non-negative, one
/// semi-transparent `POLY_FT4` (tpage 0x2C) is queued with its corners on two
/// radii at angles `arg3` and `arg3 + 0x400`, of length `arg2 * 47` divided by
/// the depth. The low 12 bits of `arg1` pick a 48x48 cell of a five-column
/// texture grid, and any bit above them selects CLUT 0x428F instead of 0x43D0.
void func_shelter_b1_pod_service_gantry_8017E400(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                bank;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;

    bank     = arg1 >> 12;
    arg1    &= 0xFFF;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (RoomDraw19Scratch*)(head - 0x1C);
    Mem_Set(block, 0, 0x1C);
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    block->vec.vz                               = *(u16*)&arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
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
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        ang2      = ang + 0x400;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
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
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b1_pod_service_gantry_8017ED3C(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            } else {
                func_shelter_b1_pod_service_gantry_8017F160(coord, work->field_20, (s16)work->field_24);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            state          = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                kind           = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12_real();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            return;
        case 1:
            func_shelter_b1_pod_service_gantry_8017ED3C(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            func_shelter_b1_pod_service_gantry_8017F160(coord, work->field_20, (s16)work->field_24);
            break;
        default:
            return;
    }
    if ((s16)work->field_2A != 0) {
        coord->coord.t[0] += work->field_10.vx;
        coord->coord.t[1] += work->field_10.vy;
        coord->coord.t[2] += work->field_10.vz;
        coord->flg         = 0;
        work->field_10.vy += 6;
    }
    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent raw-tex
/// `POLY_FT4` (tpage 0x2C, clut 0x43D3) rotated about the projected centre.
/// `arg1` selects a 32-texel UV column on the 0xE0..0xFF texture row. The
/// on-screen radius is `arg2 * 31 / otz`, and `arg3` is the spin angle,
/// applied at `arg3` and `arg3 + 0x400` through `rsin`/`rcos`. The scratch
/// block is zeroed with `Mem_Set` before use.
void func_shelter_b1_pod_service_gantry_8017ED3C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**                                scratch;
    u8*                                   head;
    ShelterB1PodServiceGantrySpinScratch* block;
    POLY_FT4*                             prim;
    s32                                   u0;
    s32                                   u1;
    s32                                   v;
    s32                                   ang;
    s32                                   ang2;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (ShelterB1PodServiceGantrySpinScratch*)(head - 0x1C);
    *scratch = block;
    Mem_Set(block, 0, 0x1C);
    block->vec.vx = *(u16*)&arg0->workm.t[0];
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    block->vec.vz = *(u16*)&arg0->workm.t[2];
    arg0          = (GsCOORDINATE2*)block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((ShelterB1PodServiceGantrySpinScratch*)(head - 0x1C))->vec);
    gte_rtps_real();
    gte_stsxy(&((ShelterB1PodServiceGantrySpinScratch*)(head - 0x1C))->sx);
    gte_stflg(&((ShelterB1PodServiceGantrySpinScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((ShelterB1PodServiceGantrySpinScratch*)arg0)->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = 0x43D3;
        u0          = arg1 << 5;
        v           = 0xE0;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, v, u1, v, u0, 0xFF, u1, 0xFF);
        block->dx = (((arg2 * 31) / ((ShelterB1PodServiceGantrySpinScratch*)(head - 0x1C))->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 31) / ((ShelterB1PodServiceGantrySpinScratch*)(head - 0x1C))->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        ang2      = ang + 0x400;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        block->dx = (((arg2 * 31) / ((ShelterB1PodServiceGantrySpinScratch*)(head - 0x1C))->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / ((ShelterB1PodServiceGantrySpinScratch*)(head - 0x1C))->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)((ShelterB1PodServiceGantrySpinScratch*)(head - 0x1C))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Projects the coordinate's world position through `GsWSMATRIX` into a
/// 0x18-byte scratch block zeroed with `Mem_Set` and, when the GTE flag is
/// non-negative, queues one shade-tex `POLY_FT4` (tpage 0x2B, clut 0x4393)
/// with a 56-texel UV tile picked by `arg1` and an on-screen radius of
/// `arg2 * 55 / otz`.
void func_shelter_b1_pod_service_gantry_8017F160(GsCOORDINATE2* arg0, u16 arg1, s16 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    u32                cell;
    s32                u1;
    s32                v0;
    s32                v1;
    s32                sarg;
    s32                t;
    s16                xy;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (RoomDraw14Scratch*)(head - 0x18);
    *scratch = block;
    Mem_Set(block, 0, 0x18);
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    block->vec.vz                               = *(u16*)&arg0->workm.t[2];
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        cell        = arg1;
        prim->clut  = 0x4393;
        tex         = (cell & 3) * 0x38;
        SOFT_BARRIER();
        v0            = ((cell & 7) >> 2) * 0x38;
        u1            = tex + 0x37;
        prim->v0      = v0;
        prim->v1      = v0;
        v1            = v0 + 0x37;
        prim->u1      = u1;
        prim->u3      = u1;
        sarg          = arg2;
        prim->v2      = v1;
        prim->v3      = v1;
        t             = sarg * 0x38;
        prim->u0      = tex;
        prim->u2      = tex;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x0 = prim->x2 = xy;
        xy                  = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x1 = prim->x3 = xy;
        xy                  = (*(u16*)&block->sy - *(u16*)&block->radius) - (block->radius >> 1);
        ds                  = &gDisplayState;
        prim->y0 = prim->y1 = xy;
        xy                  = *(u16*)&block->sy + (block->radius >> 1);
        prim->y2 = prim->y3 = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Draws a glowing disc at the point (0, -0xC4, 0) in `arg0`'s local frame:
/// the point is rotated by `workm`, offset by its translation and projected
/// through `GsWSMATRIX` into a zeroed scratch block popped from
/// `G_SCRATCH_HEAD`. When the GTE flag is non-negative, four Gouraud
/// `POLY_G4` quarter-wedges of radius `arg2 * 64 / otz` are queued, each lit
/// at the centre vertex and black on the rim. `arg3` packs the centre colour
/// as three 4-bit channels (red in bits 8-11, green 4-7, blue 0-3); a one-bit
/// flicker, taken from the global at 0x801752EC plus the per-slot byte
/// `arg1 & 7` of this room's random table, is shifted left by `arg3`'s top
/// nibble and added to every channel.
void func_shelter_b1_pod_service_gantry_8017F450(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                blend;
    s32                color;
    u16                color16;
    u32                c;
    s32                green;
    u8                 red;

    head                    = *(u8**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = head - 0x18;
    block                   = *(RoomDraw14Scratch**)G_SCRATCH_HEAD;
    color                   = arg3;
    color16                 = color;
    Mem_Set(block, 0, 0x18);
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = 0;
    block->vec.vy                               = -0xC4;
    block->vec.vz                               = 0;
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(block);
    gte_rtv0_real();
    gte_stsv(block);
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&((RoomDraw14Scratch*)(head - 0x18))->vec.vx + *(u16*)&arg0->workm.t[0];
    block->vec.vy                               = *(u16*)&block->vec.vy + *(u16*)&arg0->workm.t[1];
    block->vec.vz                               = *(u16*)&block->vec.vz + *(u16*)&arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        arg2          = ((s16)arg2 * 64) / block->otz;
        ang           = 0;
        blend         = (D_801752EC + (u8)D_shelter_b1_pod_service_gantry_8018256C[arg1 & 7]) & 1;
        c             = color16;
        blend       <<= c >> 12;
        red           = blend + ((c >> 4) & 0xF0);
        green         = blend + (c & 0xF0);
        arg3          = blend + ((arg3 & 0xF) << 4);
        block->radius = arg2;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, red, green, arg3);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}
