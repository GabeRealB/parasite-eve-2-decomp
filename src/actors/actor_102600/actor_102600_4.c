#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102600_80131E4C;

void func_actor_102600_80135BF0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_102600_80131E4C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
