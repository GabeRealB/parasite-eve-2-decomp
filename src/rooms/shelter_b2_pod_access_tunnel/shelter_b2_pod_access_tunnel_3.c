#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// The bytes of `rtps` and `gpf12` as this build emits them, with their two
/// leading hazard nops.
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32 Gp_LcgState;

void func_shelter_b2_pod_access_tunnel_8017ED5C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b2_pod_access_tunnel_8017F1BC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);

/// An animated sprite effect, drawn through
/// `func_shelter_b2_pod_access_tunnel_8017ED5C` (state 1) or
/// `func_shelter_b2_pod_access_tunnel_8017F1BC` (state 2, when the spawn
/// argument is negative). The first tick unpacks the spawn argument: the low
/// 12 bits are the sprite size, bits 12..14 the ticks per animation cell (1
/// when zero) and bits 28..30 the drawer's palette bank. When the work block
/// arrives without a velocity, bits 24..27 choose how one is rolled from
/// `Gp_LcgState` (0 leaves the sprite still) and it is scaled to the speed in
/// bits 16..23 (0x40 when zero). Each later tick draws the current cell, moves
/// the coordinate and bends the vertical velocity, and releases the work block
/// after the drawer's last cell (12 or 10). While the room is in an event it
/// only draws, and releases once the event state reaches 4.
void func_shelter_b2_pod_access_tunnel_8017E6E0(Task* task)
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
            func_shelter_b2_pod_access_tunnel_8017ED5C(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
        } else {
            func_shelter_b2_pod_access_tunnel_8017F1BC(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
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
            func_shelter_b2_pod_access_tunnel_8017ED5C(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
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
            func_shelter_b2_pod_access_tunnel_8017F1BC(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
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

/// Draws one cell of a 5-column, 48-texel sprite sheet (tpage 0x2B) as a
/// semi-transparent `POLY_FT4` centred on the coordinate's projected position.
/// `arg1`'s low 12 bits are the cell index and its top nibble the palette
/// bank, `arg2` the half-extent (scaled by 47 over depth) and `arg3` the
/// quad's rotation. Nothing is drawn when the projection fails.
void func_shelter_b2_pod_access_tunnel_8017ED5C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    u16                bank;
    u32                idx;

    head                                        = *(u8**)G_SCRATCH_HEAD;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                     = head - 0x1C;
    block                                       = *(RoomDraw19Scratch**)G_SCRATCH_HEAD;
    block->vec.vy                               = arg0->workm.t[1];
    block->vec.vz                               = arg0->workm.t[2];
    idx                                         = arg1;
    idx                                        &= 0xFFF;
    bank                                        = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
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
        setUV4(prim, u0, v0 + 0x68, u0 + 0x2F, v0 + 0x68, u0, v0 - 0x69, u0 + 0x2F, v0 - 0x69);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
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

/// The same sprite drawer as `func_shelter_b2_pod_access_tunnel_8017ED5C` for
/// the sheet on tpage 0x2C, with one of two fixed palettes chosen by the top
/// nibble of `arg1`.
void func_shelter_b2_pod_access_tunnel_8017F1BC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    u16                bank;
    u16                vz;

    bank                                        = arg1 >> 12;
    arg1                                       &= 0xFFF;
    head                                        = *(u8**)G_SCRATCH_HEAD;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                     = head - 0x1C;
    block                                       = *(RoomDraw19Scratch**)G_SCRATCH_HEAD;
    block->vec.vy                               = arg0->workm.t[1];
    block->vec.vz                               = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
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
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
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
