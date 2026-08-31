#include "common.h"
#include "main/task.h"

void Room_Script18(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() != 0xC) {
            GameFlag_SetNibble(arg0->spawnArg1, 2);
        }
        Task_Kill(arg0);
    }
}
