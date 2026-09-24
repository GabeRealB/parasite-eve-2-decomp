#include "common.h"

#include "actors/actor_401000.h"
#include "gameplay/1BC.h"

extern GpEnemyTaskFuncTable3 D_actor_401000_8013207C;

/// Runs the actor's handler for the task's current state, copying the
/// three-entry table onto the stack first.
void func_actor_401000_8013E038(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_401000_8013207C;
    sp.funcs[task->state](task->spawnArg2, task);
}
