#include "common.h"
#include "actors/actor_110700.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The actor's task entry. Runs the handler for the task's current state,
/// passing the `GpEnemy` the task was spawned with: state 0 sets the actor up,
/// state 1 is its per-frame update. The two-entry handler table is built on
/// the stack on every call.
void func_actor_110700_80131E24(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_110700_80131E78,
        func_actor_110700_80131F44,
    };

    fns[task->state](task->spawnArg2, task);
}
