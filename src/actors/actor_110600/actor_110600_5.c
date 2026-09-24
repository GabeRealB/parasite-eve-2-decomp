#include "common.h"

#include "actors/actor_110600.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Runs the enemy task's current state handler from the actor's three-entry
/// table (spawn, per-frame tick, teardown), copying the table onto the stack
/// before the call.
void func_actor_110600_80138EA8(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_110600_80131FA0;
    sp.funcs[task->state](task->spawnArg2, task);
}
