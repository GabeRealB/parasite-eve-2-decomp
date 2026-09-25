#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Task handler of the actor whose work block this overlay publishes: runs
/// the handler for the task's state from a two-entry table built on the stack
/// (0 spawns, 1 runs a frame), refreshing `D_actor_451100_8014E744` from the
/// task's work slot first so the handlers can reach the block without the
/// task.
void func_actor_451100_801322D4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_451100_80131E24,
        func_actor_451100_80132330,
    };

    D_actor_451100_8014E744 = (Actor451100Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}
