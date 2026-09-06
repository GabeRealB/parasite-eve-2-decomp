#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/lifedrain.h"

extern s32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0` / `gpf 1`. The `inline_c.h` macros of those names
/// assemble to different words, so spell the instructions out.
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Runs one frame of a life-drain mote. Any state releases the work block once
/// the player is dying (`Gp_StateC08.field_3 == -2`) or the room is fading
/// (`Gp_State1C->field_E >= 4`).
///
/// State 0 reparents the mote onto the cast's collector task
/// `D_lifedrain_80130B0C`, hands it this task's `spawnArg1`, and draws a random
/// drift out of three LCG steps: `field_10` / `field_14` in `0x40 - [0, 0x7F]`
/// and `field_12` in `0xFFE0 - [0, 0x3F]`, so the mote starts moving up and
/// away. `field_24` is the combo level and `field_26` the wedge radius from
/// `D_lifedrain_80130AB4`, `field_28` trailing it by `0x100`.
///
/// State 1 walks the coordinate by that drift and, every other tick, draws a
/// wedge through `func_lifedrain_801301AC` and one time in four parents a
/// `0x600AD` spark. On tick 0xF it aims: the player's part-1 translation minus
/// its own, rotated into the mote's frame by `workm` and by `coord`, becomes
/// the unit heading in `field_18`, scaled by GPF with `0x1200 / (0x1E - tick)`
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
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((Gp_StateC08.field_3 != -2) && (Gp_State1C->field_E < 4)) {
        mem->field_22 = (u16)mem->field_22 + 1;
        switch (arg0->state) {
            case 0:
                Task_Reparent(D_lifedrain_80130B0C, arg0);
                D_lifedrain_80130B0C->spawnArg1 += arg0->spawnArg1;
                Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
                mem->field_10                    = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
                mem->field_12                    = 0xFFE0 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState                      = Gp_LcgState * 5 + 0x71357911;
                mem->field_14                    = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                arg0->state                      = 1;
                mem->field_24                    = (Gp_StateC08.field_0 % 10) - 1;
                val                              = D_lifedrain_80130AB4[mem->field_2A].unk6;
                mem->field_26                    = val;
                mem->field_28                    = val - 0x100;
                /* fallthrough */
            case 1:
                coord->coord.t[0] += mem->field_10;
                coord->coord.t[1] += mem->field_12;
                coord->coord.t[2] += mem->field_14;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (mem->field_22 & 1) {
                    mem->field_20 = (u16)mem->field_20 + 1;
                    func_lifedrain_801301AC(coord, mem->field_20, mem->field_28);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(0x600AD, coord, mem->field_26, NULL);
                        if (spawned != NULL) {
                            Task_Reparent(arg0, spawned->field_0);
                        }
                    }
                }
                if (mem->field_22 == 0xF) {
                    player = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[1];
                    vec.vx = player->workm.t[0] - coord->workm.t[0];
                    vec.vy = player->workm.t[1] - coord->workm.t[1];
                    vec.vz = player->workm.t[2] - coord->workm.t[2];
                    ApplyTransposeMatrixLV(&coord->workm, &vec, &vec);
                    mem->field_18 = vec.vx;
                    mem->field_1A = vec.vy;
                    mem->field_1C = vec.vz;
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&mem->field_18);
                    gte_rtv0_real();
                    gte_stsv(&mem->field_18);
                    gte_lddp(0x1200 / (0x1E - mem->field_22));
                    gte_ldsv(&mem->field_18);
                    gte_gpf12_real();
                    gte_stsv(&mem->field_18);
                    arg0->state = 2;
                }
                return;
            case 2:
                cur           = mem->field_10;
                mem->field_10 = (cur < mem->field_18) ? cur + 0x10 : cur - 0x10;
                cur           = mem->field_12;
                mem->field_12 = (cur < mem->field_1A) ? cur + 0x10 : cur - 0x10;
                cur           = mem->field_14;
                mem->field_14 = (cur < mem->field_1C) ? cur + 0x10 : cur - 0x10;

                coord->coord.t[0] += mem->field_10;
                coord->coord.t[1] += mem->field_12;
                coord->coord.t[2] += mem->field_14;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (mem->field_22 >= 0x1E) {
                    break;
                }
                if (mem->field_22 & 1) {
                    mem->field_20 = ((u16)mem->field_20 + 1) & 3;
                    func_lifedrain_801301AC(coord, mem->field_20, mem->field_28);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                        spawned = Gp_SpawnEff(0x600AD, coord, mem->field_26, NULL);
                        if (spawned != NULL) {
                            Task_Reparent(arg0, spawned->field_0);
                        }
                    }
                }
                player = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[1];
                vec.vx = player->workm.t[0] - coord->workm.t[0];
                vec.vy = player->workm.t[1] - coord->workm.t[1];
                vec.vz = player->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &vec, &vec);
                mem->field_18 = vec.vx;
                mem->field_1A = vec.vy;
                mem->field_1C = vec.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&mem->field_18);
                gte_rtv0_real();
                gte_stsv(&mem->field_18);
                gte_lddp(0x1200 / (0x1E - mem->field_22));
                gte_ldsv(&mem->field_18);
                gte_gpf12_real();
                gte_stsv(&mem->field_18);
                return;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/lifedrain/lifedrain_2", func_lifedrain_801301AC);
