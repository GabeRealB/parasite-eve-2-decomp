#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 Actor04600_D0003C;

/// The enemy's four main-body handlers, dispatched through by state.
extern GpEnemyTaskFuncTable4 Actor04600_D00010;

void Actor04600_Fn02C6C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor04600_D00010;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
