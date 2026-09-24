#include "common.h"

#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/3A34.h"
#include "actors/actor_206100.h"

/// Teardown state of the beam child: each frame it clears the root
/// coordinate's `flg` and counts `killCountdown` up, and on the twelfth frame
/// unlinks the beam's collision object and kills the task.
void func_actor_206100_8014EFC8(Task* task)
{
    Actor206100ChildWork* child;
    TmdObject*            tmd;
    u16                   countdown;

    child               = (Actor206100ChildWork*)task->work;
    tmd                 = (TmdObject*)task->extra;
    tmd->coords->flg    = 0;
    countdown           = task->killCountdown + 1;
    task->killCountdown = countdown;
    if ((s16)countdown >= 0xC) {
        Gp_UnlinkObj(&child->obj);
        taskKill(task);
    }
}
