#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

void Actor05700_Fn031BC(Task* arg0);
void Actor05700_Fn035FC(Task* arg0);
void ActorsShared80136a28(Task* arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
const GpEnemyTaskFuncTable3 Actor05700_D0008C = {
    Actor05700_Fn031BC,
    Actor05700_Fn035FC,
    ActorsShared80136a28,
};

void Actor05700_Fn0517C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D0008C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
