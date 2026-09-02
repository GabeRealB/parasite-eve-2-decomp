#include "common.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_8017DC58);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_8017DC9C);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_8017DD38);

void func_mine_cavern_8017DD6C(Task* task)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 0xB) {
            GameFlag_SetNibble(0xC4, 1);
            GameFlag_SetNibble(0xBE, 2);
            GameFlag_SetNibble(0xC3, 0);
        }
        if (Gp_GetCapEventKey() == 0x15) {
            GameFlag_SetNibble(0xBB, 1);
            GameFlag_SetNibble(0x1B9, 0);
        }
        Task_Kill(task);
    }
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_8017DDFC);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_8017DEE4);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_8017DF54);

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_8017DFAC);

void func_mine_cavern_8017E088(s16 arg0)
{
    Gp_StartCapSlot(arg0, 1, 1);
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_8017E0B4);

void func_mine_cavern_8017E0F4(s32 arg0)
{
    if (arg0 != 0) {
        Game_Session->field_69 &= 0xFD;
        return;
    }
    Game_Session->field_69 |= 2;
    Game_Session->field_69 |= 8;
}
