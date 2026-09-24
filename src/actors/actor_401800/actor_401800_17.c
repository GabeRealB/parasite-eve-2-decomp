#include "common.h"

#include "actors/actor_401800.h"
#include "gameplay/1BC.h"

extern GpEnemyTaskFuncTable3 D_actor_401800_80132064;

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401800_8013E68C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_401800_80132064;
    sp.funcs[task->state](task->spawnArg2, task);
}
