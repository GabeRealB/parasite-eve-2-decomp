#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actor_215100.h"

/// Two-state dispatcher, its handler table built on the stack: state 0 spawns
/// the actor, state 1 runs it. Both handlers take the task's `GpEnemy` as
/// well as the task.
void func_actor_215100_8014CA2C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_215100_8014C660,
        func_actor_215100_8014CA80,
    };

    fns[task->state](task->spawnArg2, task);
}
