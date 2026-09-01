#include "common.h"

#include "main/session.h"

void func_80179FC8(s32 arg0, s32 arg1);

extern s8 D_80071090;
extern s8 D_801156F9;

/// Set by `func_mist_r18_8017D960` when the alternate cutscene branch ran.
extern s32 D_mist_r18_80186EA0;

void func_mist_r18_8017EC98(void)
{
    if (Game_Session->field_4D != 1) {
        D_801156F9 = 0;
    }
}

void func_mist_r18_8017ECC0(s8 arg0)
{
    D_80071090 = arg0;
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_4", func_mist_r18_8017ECCC);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_4", func_mist_r18_8017ECF4);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_4", func_mist_r18_8017ED64);
