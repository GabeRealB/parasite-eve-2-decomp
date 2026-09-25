#include "common.h"

#include "gameplay/1BC.h"

#include "main/task.h"

/// The enemy's three state handlers - spawn, per-frame tick and teardown.
extern GpEnemyTaskFuncTable3 D_actor_511000_80131E6C;

/// Runs the enemy's current state handler, copying the table onto the stack
/// before the call.
void func_actor_511000_80133D90(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_511000_80131E6C;
    sp.funcs[task->state](task->spawnArg2, task);
}
