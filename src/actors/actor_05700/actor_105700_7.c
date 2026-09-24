#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

void Actor05700_Fn052CC(GpEnemy* arg0, Task* task);
void Actor05700_Fn05310(Task* arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
const GpEnemyTaskFuncTable3 Actor05700_D00098 = {
    Actor05700_Fn052CC,
    Actor05700_Fn05310,
    Gp_DestroyEnemy,
};

void Actor05700_Fn05270(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D00098;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
