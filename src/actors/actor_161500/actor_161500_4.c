#include "common.h"

#include "actors/actor_161500.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The actor's task body: dispatches on `Task::state` to the spawn routine
/// (state 0) or the per-frame body (state 1), handing each the task's
/// `GpEnemy` from `Task::spawnArg2`. The handler table is built on the stack.
void func_actor_161500_801326E8(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_161500_80132394,
        func_actor_161500_8013273C,
    };

    fns[task->state](task->spawnArg2, task);
}
