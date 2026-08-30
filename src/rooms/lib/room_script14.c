#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/task.h"

void Room_Script14(Task* task)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 1) {
            Gp_MarkPlayTime();
        }
        if (Gp_HasCollectedBit(0x119) != 0 && Gp_HasCollectedBit(0x11A) != 0) {
            Gp_ClearCollectedBit(0x11A);
        }
        Task_Kill(task);
    }
}
