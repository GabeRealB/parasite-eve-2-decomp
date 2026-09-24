#include "common.h"
#include "main/task.h"
#include "main/gameflag.h"
#include "gameplay/3CD8.h"

void func_dryfield_factory_80180920(Task* arg0)
{
    if (GameFlag_GetNibble(0x47) == 0) {
        Gp_RunCapCmd1(arg0->spawnArg1);
    }
    taskKill(arg0);
}

void func_dryfield_factory_80180964(Task* task)
{
    switch (task->state) {
        case 0:
            GameFlag_SetNibble(0x4E, 1);
            task->killCountdown = 0x3C;
            task->state         = task->state + 1;
            return;
        case 2:
            Gp_RunCapCmd1(task->spawnArg1);
            GameFlag_SetNibble(0x4E, 0);
            task->killCountdown = 0x1E;
            task->state         = task->state + 1;
            return;
        case 1:
        case 3:
            if (--task->killCountdown < 0) {
                task->state = task->state + 1;
            }
            return;
        default:
            Gp_MsgPlayerWeapon(1);
            Gp_MsgAllyWeapon(1);
            taskKill(task);
            return;
    }
}
