#include "common.h"

#include "actors/actor_323000.h"
#include "gameplay/1BC.h"

/// Task body of the actor's descriptor: runs the handler for the task's
/// state from a stack copy of `D_actor_323000_80161E34` - the spawn handler,
/// the per-frame driver, then `Gp_DestroyEnemy`.
void func_actor_323000_80164CE4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_323000_80161E34;
    sp.funcs[task->state](task->spawnArg2, task);
}
