#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_801511c8.h"
#include "main/task.h"

/// Counts the task's kill countdown down and runs its exit callback once it
/// runs out.
void Actor07000_Fn068B4(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown - 1;
    arg0->killCountdown = temp_v0;
    if ((temp_v0 << 0x10) <= 0) {
        Task_CallExit(arg0);
    }
}

/// Exit callback of the contact effect: takes its render node back off the
/// object list and kills the task.
void Actor07000_Fn068F0(Task* arg0)
{
    Gp_UnlinkObj(&((ActorShared801511c8Work*)arg0->work)->obj);
    taskKill(arg0);
}
