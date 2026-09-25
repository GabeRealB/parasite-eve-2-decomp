#include "common.h"

#include "actors/actor_535700.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The first enemy's task body: publishes the task's work block in
/// `D_actor_535700_80146844`, then runs the handler for the task's state from
/// a table built on the stack - the spawn handler `func_actor_535700_80131FA0`,
/// then the per-frame `func_actor_535700_801324D4`.
void func_actor_535700_80132478(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_535700_80131FA0,
        func_actor_535700_801324D4,
    };

    D_actor_535700_80146844 = task->work;
    fns[task->state](task->spawnArg2, task);
}
