#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

void func_necrosis_8012F6EC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_necrosis_8012FE64(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_necrosis_80130288(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);

INCLUDE_ASM("pe/nonmatchings/necrosis/necrosis", func_necrosis_8012EF34);

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
