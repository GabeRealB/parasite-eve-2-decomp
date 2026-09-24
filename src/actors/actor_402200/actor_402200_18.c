#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actor_402200.h"

/// The enemy task's three state handlers: spawn/setup, per-frame tick and
/// teardown.
extern GpEnemyTaskFuncTable3 D_actor_402200_80131F18;

/// Runs the enemy task's current state handler from
/// `D_actor_402200_80131F18`, copying the table onto the stack first.
void func_actor_402200_80138340(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_402200_80131F18;
    sp.funcs[task->state](task->spawnArg2, task);
}
