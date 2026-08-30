#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"

void Room_Script23(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(task->spawnArg1);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
        advance:
            task->state += 1;
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xF) {
                Gp_MsgPlayerWeapon(1);
                Task_Kill(task);
            } else {
                task->state = 0;
            }
            break;
    }
}
