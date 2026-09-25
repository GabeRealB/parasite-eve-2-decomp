#include "common.h"

#include "actors/actor_151000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's task body: publishes the task's work block in
/// `D_actor_151000_8013D37C`, then runs the handler for the task's state from a
/// table built on the stack - the spawn handler `func_actor_151000_80131F1C`,
/// then the per-frame `func_actor_151000_80132450`.
void func_actor_151000_801323F4(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_151000_80131F1C,
        func_actor_151000_80132450,
    };

    D_actor_151000_8013D37C = task->work;
    fns[task->state](task->spawnArg2, task);
}
