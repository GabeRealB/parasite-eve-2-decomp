#include "common.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/task.h"
#include "main/gameflag.h"

extern TaskDesc Room_Script14Desc;

void Room_Script14(Task* task)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 1) {
            Gp_MarkPlayTime();
        }
        if (Gp_HasCollectedBit(0x119) != 0 && Gp_HasCollectedBit(0x11A) != 0) {
            Gp_ClearCollectedBit(0x11A);
        }
        taskKill(task);
    }
}

s32 Room_Snd01(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x52020008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x52020009, 0, 0);
            /* fallthrough */
        case 0xC:
            Gp_EnqueueStageSnd6(0x5202000C, 0, 0);
            break;
        case 0x65:
            if (Gp_GetCapEventKey() == 1) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
        case 0x78:
            if (Gp_GetCapEventKey() == 0) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
    }
    return 0;
}

s32 RoomsShared8017dfc8(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x7B) >= 2) {
            Gp_AgeFlag119();
            if (Gp_HasCollectedBit(0x119) == 0) {
                Gp_SetCurBit2Flag(0x1B, 1);
            }
            Gp_SpawnIfCapIdle(1, 1);
            Task_SpawnFromTable(&Room_Script14Desc, 0, 0, 0);
        } else {
            Gp_SpawnIfCapIdle(0x14, 1);
        }
    }
    return 0;
}
