#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 Actor02400_D0003C;

void Actor02400_Fn03358(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02400_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
