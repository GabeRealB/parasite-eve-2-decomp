#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Task handler of the actor whose work block lives only on its task, entry 0
/// of `D_actor_451100_8014E6E4`: runs the handler for the task's state from a
/// two-entry table built on the stack (0 spawns, 1 runs a frame), passing the
/// task's `GpEnemy` as well as the task.
void func_actor_451100_80132BD4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_451100_801328A8,
        func_actor_451100_80132C28,
    };

    fns[task->state](task->spawnArg2, task);
}
