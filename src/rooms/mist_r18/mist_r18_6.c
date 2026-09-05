#include "common.h"

#include "gameplay/3CD8.h"

#include "main/session.h"
#include "main/task.h"

void      func_80179FC8(s32 arg0, s32 arg1);
extern s8 D_80071090;
extern s8 D_801156F9;
/// Set by `func_mist_r18_8017D960` when the alternate cutscene branch ran.
extern s32 D_mist_r18_80186EA0;
extern s32 D_mist_r18_8018522C;
extern s32 D_mist_r18_8018639C;
extern s32 D_mist_r18_80186E90;
extern s32 D_mist_r18_80186E94;
extern s32 D_mist_r18_80186E98;
extern s32 D_mist_r18_80186E9C;

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

void func_mist_r18_8017ECCC(void)
{
    func_80179FC8(0, D_mist_r18_80186EA0);
}

void func_mist_r18_8017ECF4(Task* arg0)
{
    D_mist_r18_80186E90 = 0;
    D_mist_r18_80186E94 = 0;
    D_mist_r18_80186E98 = 0;
    Game_SetPtrSlot(arg0, 7);
    func_800E8634((s32)&D_mist_r18_8018522C, 0, (s32)&D_mist_r18_8018639C);
    arg0->state         = (s32)(arg0->state + 1);
    D_mist_r18_80186E9C = 1;
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_6", func_mist_r18_8017ED64);
