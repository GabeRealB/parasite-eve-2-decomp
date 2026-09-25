#include "common.h"

#include "actors/actor_311900.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Runs the actor's second state table - `func_actor_311900_801624F8`'s setup,
/// `func_actor_311900_801625F0`'s tick and `Gp_DestroyEnemy` - at the handler
/// `Task::state` selects. The table is copied onto the stack before the call,
/// the same shape as `func_actor_311900_8016222C` for the first table.
void func_actor_311900_8016249C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_311900_80161E30;
    sp.funcs[task->state](task->spawnArg2, task);
}
