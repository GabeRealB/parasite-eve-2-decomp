#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "actors/actor_403600.h"

/// The actor's task entry: runs the handler for `task->state` from a two-entry
/// table built on the stack, passing the enemy the task was spawned for and
/// the task. State 0 is the spawn (`func_actor_403600_80138EF8`, which
/// advances the state), state 1 the per-frame update.
void func_actor_403600_80141180(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_403600_80138EF8,
        (void (*)(GpEnemy*, Task*))func_actor_403600_8013938C,
    };

    fns[task->state](task->spawnArg2, task);
}
