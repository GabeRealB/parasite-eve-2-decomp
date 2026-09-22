#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

void Actor05600_Fn03924(Task* arg0);
void Actor05600_Fn03EBC(Task* arg0);
void Actor05600_Fn01A4C(Task* arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
const GpEnemyTaskFuncTable3 Actor05600_D00098 = {
    Actor05600_Fn03924,
    Actor05600_Fn03EBC,
    Actor05600_Fn01A4C,
};

void Actor05600_Fn04CA0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05600_D00098;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
