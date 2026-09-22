#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

void ActorsShared80135b64(Task* arg0);
void Actor02300_Fn03D88(Task* arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
const GpEnemyTaskFuncTable3 Actor02300_D0006C = {
    ActorsShared80135b64,
    Actor02300_Fn03D88,
    Gp_DestroyEnemy,
};

void Actor02300_Fn03CE8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D0006C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
