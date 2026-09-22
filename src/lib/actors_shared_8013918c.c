#include "common.h"
#include "main/task.h"
#include "actors/actors_shared_801511c8.h"

void ActorsShared8013918c(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown - 1;
    arg0->killCountdown = temp_v0;
    if ((temp_v0 << 0x10) <= 0) {
        Task_CallExit(arg0);
    }
}

/// Task exit callback: takes the task's display node back off the object list
/// and kills the task.
void ActorsShared801511c8(Task* arg0)
{
    Gp_UnlinkObj(&((ActorShared801511c8Work*)arg0->work)->obj);
    taskKill(arg0);
}
