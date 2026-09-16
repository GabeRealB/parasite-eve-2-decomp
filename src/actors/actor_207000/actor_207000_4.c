#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 D_actor_207000_80149E30;
extern GpEnemyTaskFuncTable4 D_actor_207000_80149E5C;

void func_actor_207000_8014CB30(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_207000_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
