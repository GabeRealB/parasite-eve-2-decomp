#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/lifedrain.h"

extern s32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0` / `gpf 1` / `rtps`. The `inline_c.h` macros of those
/// names assemble to different words, so spell the instructions out.
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")

/// Runs one frame of a life-drain mote. Any state releases the work block once
/// the player is dying (`Gp_StateC08.field_3 == -2`) or the room is fading
/// (`Gp_State1C->fadeState >= 4`).
///
/// State 0 reparents the mote onto the cast's collector task
/// `D_lifedrain_80130B0C`, hands it this task's `spawnArg1`, and draws a random
/// drift out of three LCG steps: `move` / `move.vz` in `0x40 - [0, 0x7F]`
/// and `move.vy` in `0xFFE0 - [0, 0x3F]`, so the mote starts moving up and
/// away. `scale` is the combo level and `angle` the wedge radius from
/// `D_lifedrain_80130AB4`, `period` trailing it by `0x100`.
///
/// State 1 walks the coordinate by that drift and, every other tick, draws a
/// wedge through `func_lifedrain_801301AC` and one time in four parents a
/// `0x600AD` spark. On tick 0xF it aims: the player's part-1 translation minus
/// its own, rotated into the mote's frame by `workm` and by `coord`, becomes
/// the unit heading in `pos`, scaled by GPF with `0x1200 / (0x1E - tick)`
/// so later ticks pull harder. State 2 then steps each drift component 0x10
/// toward that heading every frame, re-aiming as it goes, and releases at tick
/// 0x1E.
void func_lifedrain_8012FAF8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    GpEffWork*     spawned;
    s16            val;
    s32            cur;
    VECTOR         vec;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if ((Gp_StateC08.field_3 != -2) && (Gp_State1C->fadeState < 4)) {
        mem->age = (u16)mem->age + 1;
        switch (arg0->state) {
            case 0:
                Task_Reparent(D_lifedrain_80130B0C, arg0);
                D_lifedrain_80130B0C->spawnArg1 += arg0->spawnArg1;
                Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx                     = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy                     = 0xFFE0 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz                     = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                arg0->state                      = 1;
                mem->scale                       = (Gp_StateC08.field_0 % 10) - 1;
                val                              = D_lifedrain_80130AB4[mem->step].unk6;
                mem->angle                       = val;
                mem->period                      = val - 0x100;
                /* fallthrough */
            case 1:
                coord->coord.t[0] += mem->move.vx;
                coord->coord.t[1] += mem->move.vy;
                coord->coord.t[2] += mem->move.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (mem->age & 1) {
                    mem->index = (u16)mem->index + 1;
                    func_lifedrain_801301AC(coord, mem->index, mem->period);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(0x600AD, coord, mem->angle, NULL);
                        if (spawned != NULL) {
                            Task_Reparent(arg0, spawned->task);
                        }
                    }
                }
                if (mem->age == 0xF) {
                    player = &((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[1];
                    vec.vx = player->workm.t[0] - coord->workm.t[0];
                    vec.vy = player->workm.t[1] - coord->workm.t[1];
                    vec.vz = player->workm.t[2] - coord->workm.t[2];
                    ApplyTransposeMatrixLV(&coord->workm, &vec, &vec);
                    mem->pos.vx = vec.vx;
                    mem->pos.vy = vec.vy;
                    mem->pos.vz = vec.vz;
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&mem->pos);
                    gte_rtv0_real();
                    gte_stsv(&mem->pos);
                    gte_lddp(0x1200 / (0x1E - mem->age));
                    gte_ldsv(&mem->pos);
                    gte_gpf12_real();
                    gte_stsv(&mem->pos);
                    arg0->state = 2;
                }
                return;
            case 2:
                cur          = mem->move.vx;
                mem->move.vx = (cur < mem->pos.vx) ? cur + 0x10 : cur - 0x10;
                cur          = mem->move.vy;
                mem->move.vy = (cur < mem->pos.vy) ? cur + 0x10 : cur - 0x10;
                cur          = mem->move.vz;
                mem->move.vz = (cur < mem->pos.vz) ? cur + 0x10 : cur - 0x10;

                coord->coord.t[0] += mem->move.vx;
                coord->coord.t[1] += mem->move.vy;
                coord->coord.t[2] += mem->move.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (mem->age >= 0x1E) {
                    break;
                }
                if (mem->age & 1) {
                    mem->index = ((u16)mem->index + 1) & 3;
                    func_lifedrain_801301AC(coord, mem->index, mem->period);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(0x600AD, coord, mem->angle, NULL);
                        if (spawned != NULL) {
                            Task_Reparent(arg0, spawned->task);
                        }
                    }
                }
                player = &((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[1];
                vec.vx = player->workm.t[0] - coord->workm.t[0];
                vec.vy = player->workm.t[1] - coord->workm.t[1];
                vec.vz = player->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &vec, &vec);
                mem->pos.vx = vec.vx;
                mem->pos.vy = vec.vy;
                mem->pos.vz = vec.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&mem->pos);
                gte_rtv0_real();
                gte_stsv(&mem->pos);
                gte_lddp(0x1200 / (0x1E - mem->age));
                gte_ldsv(&mem->pos);
                gte_gpf12_real();
                gte_stsv(&mem->pos);
                return;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Links two axis-aligned `POLY_FT4`s at `arg0`'s world position: the position
/// is projected through `GsWSMATRIX` by a single `RTPS` and both quads are
/// dropped when that sets a negative `gte_stflg`. The inner sprite is one of
/// four 0x18-wide frames on tpage 0x2A (CLUT 0x42C5) picked by `arg1 & 3`, sized
/// `arg2 * 23 / otz`. The outer sprite is the 0x38-wide cell on tpage 0x29 whose
/// CLUT is `0x4310 + (arg1 & 1)`, sized `((arg2 * 2) / 3) * 55 / otz`. Same
/// 0x18-byte scratch and axis-aligned corners as gameplay `Gp_EffSprTask8D`.
void func_lifedrain_801301AC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**           scratch;
    u8*              head;
    GpEffFt4Scratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s16              x;
    s16              y;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpEffFt4Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpEffFt4Scratch*)(head - 0x18);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffFt4Scratch*)(head - 0x18))->sx);
    gte_stflg(&((GpEffFt4Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffFt4Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        prim->tpage    = 0x2A;
        prim->clut     = 0x42C5;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        u0          = (arg1 & 3) * 0x18;
        u1          = u0 + 0x17;
        prim->u1    = u1;
        prim->u0    = u0;
        prim->u2    = u0;
        prim->u3    = u1;
        prim->v2    = 0x17;
        prim->v3    = 0x17;
        prim->v0    = 0;
        prim->v1    = 0;
        block->size = (arg2 * 0x17) / block->otz;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        prim->tpage    = 0x29;
        prim->clut     = ((u32)(((arg1 & 1) * 0x10) + 0x100) >> 4) | 0x4300;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->u0    = 0x38;
        prim->v0    = 0xC8;
        prim->u1    = 0x6F;
        prim->v1    = 0xC8;
        prim->v2    = 0xFF;
        prim->v3    = 0xFF;
        prim->u2    = 0x38;
        prim->u3    = 0x6F;
        block->size = ((s16)((arg2 * 2) / 3) * 0x37) / block->otz;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}
