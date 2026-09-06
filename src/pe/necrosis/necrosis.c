#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

void func_necrosis_8012F6EC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);

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

INCLUDE_ASM("pe/nonmatchings/necrosis/necrosis", func_necrosis_8012FAF8);

INCLUDE_ASM("pe/nonmatchings/necrosis/necrosis", func_necrosis_8012FE64);

INCLUDE_ASM("pe/nonmatchings/necrosis/necrosis", func_necrosis_80130288);

INCLUDE_RODATA("pe/nonmatchings/necrosis/necrosis", D_necrosis_8012EF30);
