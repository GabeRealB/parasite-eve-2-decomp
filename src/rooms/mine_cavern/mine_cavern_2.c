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
