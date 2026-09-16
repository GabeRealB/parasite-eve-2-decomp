#include "common.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern s8  D_801153F1;
extern s32 D_mine_cavern_8018EB54;

void func_mine_cavern_8017DFAC(s32 arg0)
{
    if ((GameFlag_GetNibble(0xE6) == 1 && D_mine_cavern_8018EB54 == 0) ||
        (GameFlag_GetNibble(0xE6) == 2 && D_mine_cavern_8018EB54 == 1)) {
        Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x1E);
        D_801153F1              = arg0;
        Game_Session->field_69 |= 0x80;
        D_mine_cavern_8018EB54 += 1;
        return;
    }
    if (arg0 < Gp_StateF0.field_1) {
        Gp_StateF0.field_1 = arg0;
    }
}

void func_mine_cavern_8017E088(s16 arg0)
{
    Gp_StartCapSlot(arg0, 1, 1);
}

void func_mine_cavern_8017E0B4(void)
{
    Gp_StateF0.field_0 = 0;
    if (Gp_StateF0.field_6 == 0) {
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
    }
    Gp_ArmStateF0(1);
}

void func_mine_cavern_8017E0F4(s32 arg0)
{
    if (arg0 != 0) {
        Game_Session->field_69 &= 0xFD;
        return;
    }
    Game_Session->field_69 |= 2;
    Game_Session->field_69 |= 8;
}
