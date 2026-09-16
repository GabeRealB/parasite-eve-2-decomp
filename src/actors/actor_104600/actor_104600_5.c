#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_104600_80131E5C;

/// The enemy's four main-body handlers, dispatched through by state.
extern GpEnemyTaskFuncTable4 D_actor_104600_80131E30;

void func_actor_104600_80134A8C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_104600_80131E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
