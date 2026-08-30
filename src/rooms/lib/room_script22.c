#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

void Room_Script22(Task* task)
{
    s32 flag;
    s32 cmd;

    flag = task->spawnArg1;
    cmd  = (s32)task->spawnArg2;
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(cmd);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            goto advance;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                GameFlag_SetNibble(flag, GameFlag_GetNibble(flag) == 0);
            }
        advance:
            task->state = task->state + 1;
            break;
        case 3:
            Task_Kill(task);
            break;
    }
}
