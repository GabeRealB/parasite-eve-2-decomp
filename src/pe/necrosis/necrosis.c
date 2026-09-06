#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/necrosis.h"

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0` / `gpf 1`. The `inline_c.h` macros of those names
/// assemble to different words, so spell the instructions out.
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_necrosis_8012F6EC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_necrosis_8012FE64(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_necrosis_80130288(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Runs one frame of the necrosis cast. State 0 copies the player rotation onto
/// the effect coordinate, rotates a (0, 0, 0x90) offset into that frame, and
/// links a `NecrosisWork` collision pair (list 1 + list 7) whose packed id is
/// the combo digits plus `0x28000`. State 1 GPF-scales that offset by 0x1100
/// each frame, walks the coordinate, and spawns `0x80060019`; a `0x100000` hit
/// on `obj2` zeros the offset and unlinks the list-7 object. State 2 waits
/// `field_2 + 0x10` ticks. Any state releases if the player is dying
/// (`Gp_StateC08.field_3` / `D_80114C0B`) or the room is fading (`Gp_State1C`).
void func_necrosis_8012EF34(Task* arg0)
{
    NecrosisWork*  work;
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    GpMtxWords*    dstm;
    GpMtxWords*    srcm;
    GpRec18*       rec;
    GpEffWork*     spawned;
    s32            pan;
    u16            old;
    s32            tick;
    s16            fade;

    work          = (NecrosisWork*)arg0->idMap;
    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    old           = mem->field_22;
    tick          = old + 1;
    mem->field_22 = tick;
    SOFT_USE_REG(tick);
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_3 == -2) {
                goto release;
            }
            fade = Gp_State1C->field_E;
            if (fade >= 4) {
                goto release;
            }
            if (fade != 0) {
                mem->field_22 = old;
                return;
            }
            work = Mem_Calloc(0x58, 0);
            if (work == NULL) {
                mem->field_22 = 0;
                return;
            }
            player     = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            dstm       = (GpMtxWords*)&coord->coord;
            srcm       = (GpMtxWords*)&player->coord;
            dstm->w0   = srcm->w0;
            dstm->w1   = srcm->w1;
            dstm->w2   = srcm->w2;
            dstm->w3   = srcm->w3;
            dstm->h4   = srcm->h4;
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            mem->field_10 = 0;
            mem->field_12 = 0;
            mem->field_14 = 0x90;
            gte_SetRotMatrix((MATRIX*)srcm);
            gte_ldv0(&mem->field_10);
            gte_rtv0_real();
            gte_stsv(&mem->field_10);
            rec               = &work->rec;
            mem->field_20     = (Gp_StateC08.field_0 % 10) - 1;
            arg0->idMap       = (TaskIdMap*)work;
            work->obj.field_8 = coord;
            work->obj.field_C = rec;
            work->obj.field_18 =
                ((u16)(Gp_StateC08.field_0 / 100) - 1) * 9 + ((u16)((u16)(Gp_StateC08.field_0 % 100) / 10) - 1) * 3 + (u16)(Gp_StateC08.field_0 % 10) + 0x28000;
            work->obj.field_1C = D_necrosis_801306BC[mem->field_20].field_0;
            work->obj.flags    = 1;
            Gp_LinkObj(1, &work->obj);
            rec->field_0        = 2;
            work->obj2.field_8  = coord;
            work->obj2.field_C  = rec;
            work->obj2.field_18 = 0;
            work->obj2.field_1C = 0x80;
            work->obj2.flags    = 1;
            work->obj.flags    |= 0x8000;
            Gp_LinkObj(7, &work->obj2);
            work->obj2.flags = (work->obj2.flags & 0x7FFF) | 0x4400;
            pan              = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(D_necrosis_801306C8[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)Gp_GetObjDepth((GpObj38*)coord));
            Gp_SpawnPadLerp((s16)((u16)D_necrosis_801306BC[mem->field_20].field_2 + 0xC), 0xFF, 8);
            arg0->state = 1;
            /* fallthrough */
        case 1:
            if (Gp_State1C->field_E == 0) {
                gte_lddp(0x1100);
                gte_ldsv(&mem->field_10);
                gte_gpf12_real();
                gte_stsv(&mem->field_10);
                coord->coord.t[0] += mem->field_10;
                coord->coord.t[1] += mem->field_12;
                coord->coord.t[2] += mem->field_14;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                spawned = Gp_SpawnEff(0x80060019, coord,
                                      (s16)D_necrosis_801306BC[mem->field_20].field_0 + ((s16)mem->field_22 * 0x60),
                                      NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
                work->obj.field_1C = (u16)work->obj.field_1C + 0x20;
            } else {
                mem->field_22 = (u16)mem->field_22 - 1;
            }
            if ((D_80114C0B == -2) || (Gp_State1C->field_E >= 4)) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                goto release;
            }
            if ((s16)mem->field_22 > D_necrosis_801306BC[mem->field_20].field_2) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            if (Gp_FindRec18(work->obj2.field_C, 0x100000) != 0) {
                mem->field_10 = 0;
                mem->field_12 = 0;
                mem->field_14 = 0;
                Gp_UnlinkObj(&work->obj2);
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 2:
            if (D_80114C0B == -2) {
                goto release;
            }
            if (Gp_State1C->field_E >= 4) {
                goto release;
            }
            tick = (s16)tick;
            if ((D_necrosis_801306BC[mem->field_20].field_2 + 0x10) < tick) {
            release:
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
    }
}

void func_necrosis_8012F52C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            rng;
    s32            val;
    s32            step;
    GpEffWork*     spawned;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_E != 0) {
        return;
    }

    mem->field_22 = (u16)mem->field_22 + 1;
    if (arg0->state == 0) {
        mem->field_24 = (u16)arg0->spawnArg1 & 0xFFF;
        rng           = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng;
        mem->field_26 = ((u32)rng >> 16) & 0xFFF;
        SOFT_COMPILER_BARRIER();
        val           = (u16)mem->field_24;
        step          = val;
        val           = val - 0x100;
        mem->field_28 = val;
        mem->field_2A = (s32)(step << 16) >> 20;
        arg0->state   = 1;
    }
    Gp_UpdateCoord(coord);
    func_necrosis_8012F6EC(coord, (s16)(mem->field_22 % 6), mem->field_24, mem->field_26);
    mem->field_24 = (u16)mem->field_24 - (u16)mem->field_2A;
    if ((s16)mem->field_24 < mem->field_2A) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if ((s16)(mem->field_22 % 3) == 0) {
        spawned = Gp_SpawnEff(0x6001A, coord, mem->field_28, 0);
        if (spawned != NULL) {
            Task_Reparent(arg0, spawned->field_0);
        }
    }
}

INCLUDE_ASM("pe/nonmatchings/necrosis/necrosis", func_necrosis_8012F6EC);

void func_necrosis_8012FAF8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            tick;
    s32            rng1;
    s32            rng2;
    s32            rng3;
    s32            temp_lo;
    s32            var_v1;
    u16            temp_v0;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_E != 0) {
        return;
    }

    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            mem->field_22 = 0;
            temp_v0       = arg0->spawnArg1;
            mem->field_28 = temp_v0 & 0xFFF;
            rng1          = (Gp_LcgState * 5) + 0x71357911;
            mem->field_24 = ((u32)rng1 >> 16) & 0xFFF;
            Gp_LcgState   = rng1;
            mem->field_26 = mem->field_28 / 20;
            mem->field_10 = (rsin(mem->field_24) * mem->field_26) >> 12;
            temp_lo       = rcos(mem->field_24) * mem->field_26;
            rng2          = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState   = rng2;
            mem->field_12 = temp_lo >> 12;
            mem->field_14 = (rsin(((u32)rng2 >> 16) & 0xFFF) * mem->field_10) >> 12;
            rng3          = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState   = rng3;
            if ((s32)(((u32)rng3 >> 16) & 3) < ((u16)(Gp_StateC08.field_0 % 10U) - 1)) {
                mem->field_2A = 0x1000;
            }
            if ((u16)(Gp_StateC08.field_0 % 10U) - 1 < 2) {
                arg0->state = 1;
                return;
            }
            var_v1 = 2;
            if (mem->field_2A != 0) {
                var_v1 = 1;
            }
            arg0->state = var_v1;
            return;
        case 1:
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            coord->coord.t[2] += mem->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            tick          = (u16)mem->field_20 + 1;
            mem->field_20 = tick;
            if (tick < 8) {
                func_necrosis_8012FE64(coord, (s16)(tick | (u16)mem->field_2A), mem->field_28,
                                       mem->field_24);
                return;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        case 2:
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            coord->coord.t[2] += mem->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            tick          = (u16)mem->field_20 + 1;
            mem->field_20 = tick;
            if (tick < 6) {
                func_necrosis_80130288(coord, (s16)(tick | (u16)mem->field_2A), mem->field_28,
                                       mem->field_24);
                return;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

INCLUDE_ASM("pe/nonmatchings/necrosis/necrosis", func_necrosis_8012FE64);

INCLUDE_ASM("pe/nonmatchings/necrosis/necrosis", func_necrosis_80130288);

INCLUDE_RODATA("pe/nonmatchings/necrosis/necrosis", D_necrosis_8012EF30);
