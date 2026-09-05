#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "weapons/m4a1_hammer.h"
#include "main/tmd.h"

extern u32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix, no translation.
/// The `inline_c.h` macro of that name assembles to a different word, so spell
/// the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Per-frame task for the hammer's charge flare. `Task::spawnArg2` is the
/// `Gp_State1C` work block, `Task::extra` reaches the coordinate the flare
/// hangs on, and `Task::spawnArg1` is the charge phase the firing code drives.
/// Any room fade of 2 or more, and the player being in the state flagged by
/// `TmdObject::field_C & 0x80`, freeze the task outright.
///
/// - State 0 hangs the coordinate off `GpEffWork::field_8` at the fixed offset
///   `D_m4a1_hammer_8011EB60` with an identity rotation, publishes the task as
///   `D_m4a1_hammer_8012D660` and moves to state 1.
/// - State 1 first republishes the flare's world position as
///   `D_m4a1_hammer_8012D668`, then dispatches on the charge phase. Phase 1
///   idles the flare: it re-rolls the spin angle every 16 frames and the radius
///   every frame, draws it on even frames and claims room-light slot 1 as a
///   narrow (`0x80` / `0x400`) light. Phase 2 charges: on the first frame it
///   seeds the eight sparks in `D_m4a1_hammer_8012D630`, and on every even
///   frame it walks each spark, rotates its offset through the flare's frame
///   and draws it, then widens the light to `0x400` / `0x4000`; five charge
///   frames drop back to phase 1. Phase 3 tears the flare down. A room fade
///   winds `field_22` back down and redraws instead of advancing.
void func_m4a1_hammer_8011D1E0(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* light;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GpMtxWords*    dstm;
    s32            i;
    s32            j;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    base  = &Gp_RoomCoords[1];
    light = &base->coord;
    slot  = (GpCoordTail*)light;

    if ((((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_C & 0x80) == 0 && Gp_State1C->field_4 < 2) {
        work->field_22 = (u16)work->field_22 + 1;
        switch (task->state) {
            case 0:
                dstm              = (GpMtxWords*)&coord->coord;
                coord->sub        = work->field_8;
                dstm->w0          = 0x1000;
                dstm->w2          = 0x1000;
                dstm->h4          = 0x1000;
                dstm->w1          = 0;
                dstm->w3          = 0;
                coord->coord.t[0] = D_m4a1_hammer_8011EB60.vx;
                coord->coord.t[1] = D_m4a1_hammer_8011EB60.vy;
                coord->coord.t[2] = D_m4a1_hammer_8011EB60.vz;
                coord->flg        = 0;

                D_m4a1_hammer_8012D660 = task;
                Gp_UpdateCoord(coord);
                task->state = 1;
                return;
            case 1:
                D_m4a1_hammer_8012D668.vx = coord->workm.t[0];
                D_m4a1_hammer_8012D668.vy = coord->workm.t[1];
                D_m4a1_hammer_8012D668.vz = coord->workm.t[2];
                switch (task->spawnArg1) {
                    case 0:
                        break;
                    case 1:
                        if (Gp_State1C->field_4 != 0) {
                            work->field_22 = (u16)work->field_22 - 1;
                            if ((work->field_22 & 1) == 0) {
                                func_m4a1_hammer_8011D904(coord->workm.t, work->field_22 >> 1, work->field_28,
                                                          work->field_26);
                            }
                            return;
                        }
                        Gp_UpdateCoord(coord);
                        if ((work->field_22 & 0xF) == 0) {
                            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                            work->field_26 = (Gp_LcgState >> 16) & 0xFFF;
                        }
                        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                        work->field_28 = ((Gp_LcgState >> 16) & 0xFF) + 0xC0;
                        if ((work->field_22 & 1) == 0) {
                            func_m4a1_hammer_8011D904(coord->workm.t, work->field_22 >> 1, work->field_28,
                                                      work->field_26);
                        }
                        base->field_0  = 4;
                        slot->field_58 = 0x80;
                        slot->field_5C = 0x400;
                        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                        slot->field_54 = ((Gp_LcgState >> 16) & 0x700) + 0x400;
                        slot->field_50 = (u16)slot->field_54 >> 1;
                        slot->field_52 = (u16)slot->field_54 >> 1;
                        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                        light->flg     = 0;
                        work->field_20 = 0;
                        return;
                    case 2:
                        if (Gp_State1C->field_4 != 0) {
                            work->field_22 = (u16)work->field_22 - 1;
                            if ((work->field_22 & 1) == 0) {
                                func_m4a1_hammer_8011DE60(coord, work->field_22 >> 1, work->field_28,
                                                          work->field_26);
                            }
                            return;
                        }
                        Gp_UpdateCoord(coord);
                        if (work->field_20 == 0) {
                            for (i = 0; i < 8; i++) {
                                Gp_LcgState                    = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i]      = (i << 9) + ((Gp_LcgState >> 16) & 0x1FF);
                                Gp_LcgState                    = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i + 8]  = ((Gp_LcgState >> 16) & 0x7FF) + 0x200;
                                Gp_LcgState                    = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i + 16] = (Gp_LcgState >> 16) & 0x3FF;
                            }
                        }
                        if ((work->field_22 & 0xF) == 0) {
                            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                            work->field_26 = (Gp_LcgState >> 16) & 0xFFF;
                        }
                        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                        work->field_28 = ((Gp_LcgState >> 16) & 0x3FF) + 0x400;
                        if ((work->field_22 & 1) == 0) {
                            func_m4a1_hammer_8011DE60(coord, work->field_22 >> 1, work->field_28, work->field_26);
                            for (i = 0; i < 8; i++) {
                                j                          = i + 8;
                                Gp_LcgState                = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[i] -= ((Gp_LcgState >> 16) & 0x1FF) - 0x100;
                                Gp_LcgState                = Gp_LcgState * 5 + 0x71357911;
                                D_m4a1_hammer_8012D630[j] += (Gp_LcgState >> 16) & 0xFF;
                                work->field_18 =
                                    (D_m4a1_hammer_8012D630[i + 16] * rsin(D_m4a1_hammer_8012D630[i])) >> 12;
                                work->field_1C =
                                    (D_m4a1_hammer_8012D630[i + 16] * rcos(D_m4a1_hammer_8012D630[i])) >> 12;
                                work->field_1A = D_m4a1_hammer_8012D630[j];
                                gte_SetRotMatrix(&coord->workm);
                                gte_ldv0(&work->field_18);
                                gte_rtv0_real();
                                gte_stsv(&work->field_18);
                                work->field_18 = (u16)work->field_18 + (u16)D_m4a1_hammer_8012D668.vx;
                                work->field_1A = (u16)work->field_1A + (u16)D_m4a1_hammer_8012D668.vy;
                                work->field_1C = (u16)work->field_1C + (u16)D_m4a1_hammer_8012D668.vz;
                                func_m4a1_hammer_8011E29C(coord, (SVECTOR*)&work->field_18, work->field_22, 0x280);
                            }
                        }
                        base->field_0  = 4;
                        slot->field_58 = 0x400;
                        slot->field_5C = 0x4000;
                        Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                        slot->field_54 = ((Gp_LcgState >> 16) & 0x700) + 0x800;
                        slot->field_50 = (u16)slot->field_54 >> 1;
                        slot->field_52 = (s16)(u16)slot->field_54 >> 1;
                        Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                        light->flg     = 0;
                        work->field_20 = (u16)work->field_20 + 1;
                        if (work->field_20 >= 5) {
                            task->spawnArg1 = 1;
                        }
                        return;
                    case 3:
                        Gp_ReleaseState1CMem(work, task);
                        return;
                }
                return;
        }
    }
}

INCLUDE_ASM("weapons/nonmatchings/m4a1_hammer/m4a1_hammer", func_m4a1_hammer_8011D904);

void func_m4a1_hammer_8011DD08(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    mem->field_22++;
    switch (arg0->state) {
        case 0:
            Task_Reparent(D_m4a1_hammer_8012D660, arg0);
            if (arg0->spawnArg1 != 0) {
                parent            = mem->field_8;
                coord->coord.t[0] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[2] = 0;
                coord->flg        = 0;
                coord->sub        = parent;
                Gp_UpdateCoord(coord);
                arg0->state = 1;
            }
            mem->field_24 = 0x80;
            Gp_LcgState   = (Gp_LcgState * 5) + 0x71357911;
            mem->field_26 = (Gp_LcgState >> 16) & 0xFFF;
            /* fallthrough */
        case 1:
            if (mem->field_22 & 1) {
                func_m4a1_hammer_8011DE60(coord, ++mem->field_20, 0x400, mem->field_26);
                if (mem->field_22 < 8) {
                    func_m4a1_hammer_8011E29C(coord, &D_m4a1_hammer_8012D668, mem->field_20, 0x280);
                }
            }
            if (mem->field_22 >= 0x19) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
    }
}

INCLUDE_ASM("weapons/nonmatchings/m4a1_hammer/m4a1_hammer", func_m4a1_hammer_8011DE60);

INCLUDE_ASM("weapons/nonmatchings/m4a1_hammer/m4a1_hammer", func_m4a1_hammer_8011E29C);
