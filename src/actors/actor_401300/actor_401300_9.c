#include "common.h"

#include "actors/actor_401300.h"
#include "gameplay/1BC.h"

extern GpEnemyTaskFuncTable3 D_actor_401300_8013201C;

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401300_80141F2C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_401300_8013201C;
    sp.funcs[task->state](task->spawnArg2, task);
}
