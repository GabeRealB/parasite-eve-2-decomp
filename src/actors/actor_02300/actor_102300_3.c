#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 Actor02300_D00060;

void Actor02300_Fn03BA0(void)
{
}

void Actor02300_Fn03BA8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D00060;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
