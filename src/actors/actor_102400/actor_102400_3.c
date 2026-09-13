#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102400_80131E5C;

void func_actor_102400_80135178(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_102400_80131E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
