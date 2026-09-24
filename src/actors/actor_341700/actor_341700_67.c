#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

#include "actors/actor_341700.h"

/// Runs the controlled enemy's current state handler from
/// `D_actor_341700_80162064` - spawn/setup, per-frame tick or teardown -
/// copying the table onto the stack before the call.
void func_actor_341700_8016D32C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_341700_80162064;
    sp.funcs[task->state](task->spawnArg2, task);
}
