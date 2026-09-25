#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

#include "actors/actor_160600.h"

/// The actor's task body: dispatches on `Task::state` to the spawn routine
/// (state 0) or the per-frame body (state 1), handing each the task's
/// `GpEnemy` from `Task::spawnArg2`. The handler table is built on the stack.
void func_actor_160600_801321B4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_160600_80132208,
        func_actor_160600_80131E68,
    };

    fns[task->state](task->spawnArg2, task);
}
