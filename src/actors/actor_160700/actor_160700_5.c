#include "common.h"

#include "actors/actor_160700.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Two-state dispatcher, its handler table built on the stack: state 0 spawns
/// the actor, state 1 runs it. Both handlers take the task's `GpEnemy` as
/// well as the task.
void func_actor_160700_8013233C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_160700_80131F70,
        func_actor_160700_80132390,
    };

    fns[task->state](task->spawnArg2, task);
}
