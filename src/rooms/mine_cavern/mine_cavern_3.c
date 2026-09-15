#include "common.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_3", func_mine_cavern_8017DFAC);

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
