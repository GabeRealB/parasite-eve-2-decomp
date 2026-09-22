#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 Actor02600_D0002C;

void Actor02600_Fn03DD0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02600_D0002C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
