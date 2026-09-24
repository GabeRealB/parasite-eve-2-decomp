#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

extern GpEnemyTaskFuncTable3 D_actor_421600_80131FB0;

/// The enemy task's per-frame entry: runs the handler for the task's current
/// state - spawn, tick or teardown - from a stack copy of the state table.
void func_actor_421600_8013EEC8(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_421600_80131FB0;
    sp.funcs[task->state](task->spawnArg2, task);
}
