#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 Actor07000_D00010;
extern GpEnemyTaskFuncTable4 Actor07000_D0003C;

void Actor07000_Fn02D10(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor07000_D00010;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
