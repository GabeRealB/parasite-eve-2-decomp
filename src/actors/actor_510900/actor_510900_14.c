#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - indexed by `Task::state`.
extern GpEnemyTaskFuncTable3 D_actor_510900_80131ECC;

/// Runs the enemy's current state handler, copying the table onto the stack
/// before the call.
void func_actor_510900_8013C190(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_510900_80131ECC;
    sp.funcs[task->state](task->spawnArg2, task);
}
