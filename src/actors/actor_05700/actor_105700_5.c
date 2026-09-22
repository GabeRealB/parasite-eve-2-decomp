#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 Actor05700_D00080;

void Actor05700_Fn05038(void)
{
}

void Actor05700_Fn05040(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D00080;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
