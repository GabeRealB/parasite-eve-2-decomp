#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three task-state handlers: setup, per-frame tick and teardown.
extern GpEnemyTaskFuncTable3 D_actor_356100_80161F40;

/// Runs the handler for the task's current state, copying the table onto the
/// stack before the call.
void func_actor_356100_8016A910(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_356100_80161F40;
    sp.funcs[task->state](task->spawnArg2, task);
}
