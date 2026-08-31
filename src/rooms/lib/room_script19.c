#include "common.h"
#include "main/task.h"

void Room_Script19(Task* arg0)
{
    if (GameFlag_GetNibble(0x47) == 0) {
        Gp_RunCapCmd1(arg0->spawnArg1);
    }
    Task_Kill(arg0);
}
