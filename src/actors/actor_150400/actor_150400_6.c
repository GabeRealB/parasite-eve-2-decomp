#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

#include "actors/actor_150400.h"

/// Per-frame callback of the actor's task: runs the state's handler, the spawn
/// handler `func_actor_150400_80132014` in state 0 and the per-frame update
/// `func_actor_150400_80132434` after it, passing the task's `GpEnemy`.
void func_actor_150400_801323E0(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_150400_80132014,
        func_actor_150400_80132434,
    };

    fns[task->state](task->spawnArg2, task);
}
