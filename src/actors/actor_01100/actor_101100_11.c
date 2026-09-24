#include "common.h"

#include "actors/actor_101100.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Teardown state: counts `killCountdown` down and calls the task's exit
/// callback once it reaches zero.
void Actor01100_Fn0736C(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown - 1;
    arg0->killCountdown = temp_v0;
    if ((temp_v0 << 0x10) <= 0) {
        Task_CallExit(arg0);
    }
}

/// Exit callback of the secondary tasks: takes the work block's display node
/// back off the object list and kills the task.
void Actor01100_Fn073A8(Task* arg0)
{
    Gp_UnlinkObj(&((Actor101100Work*)arg0->work)->obj);
    taskKill(arg0);
}
