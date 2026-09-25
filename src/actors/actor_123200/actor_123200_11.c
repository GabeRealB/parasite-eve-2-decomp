#include "common.h"

#include "actors/actor_123200.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Runs the handler of `D_actor_123200_80131E30` that `Task::state` selects --
/// spawn, per-frame tick or teardown -- on the enemy in `Task::spawnArg2`,
/// copying the table onto the stack before the call.
void func_actor_123200_801341A8(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_123200_80131E30;
    sp.funcs[task->state](task->spawnArg2, task);
}
