#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

void Actor05600_Fn031B0(Task* arg0);
void Actor05600_Fn035F0(Task* arg0);
void ActorsShared80136a28(Task* arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
const GpEnemyTaskFuncTable3 Actor05600_D0008C = {
    Actor05600_Fn031B0,
    Actor05600_Fn035F0,
    ActorsShared80136a28,
};

void Actor05600_Fn04BAC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05600_D0008C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
