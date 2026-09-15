#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

s32 func_mine_cavern_8017DC58(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 6) && (GameFlag_GetNibble(0xC4) == 1)) {
        Gp_RunCapCmd1(6);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/mine_cavern/mine_cavern_2", func_mine_cavern_8017DC9C);

s32 func_mine_cavern_8017DD38(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xD) {
        SndEvt_EnqueueType6(0x54020000 | 0xD, 0, 0);
    }
    return 0;
}

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
