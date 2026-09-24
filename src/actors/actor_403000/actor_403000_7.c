#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers: spawn, per-frame tick and teardown.
extern GpEnemyTaskFuncTable3 D_actor_403000_80132004;

/// The enemy task's per-frame entry: runs the handler for the task's current
/// state - spawn, tick or teardown - from a stack copy of the state table.
void func_actor_403000_8013D59C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403000_80132004;
    sp.funcs[task->state](task->spawnArg2, task);
}
