#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_205700_80149EB8;

INCLUDE_RODATA("actors/nonmatchings/actor_205700/actor_205700_4", D_actor_205700_80149EB8);

void func_actor_205700_8014F090(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_205700_80149EB8;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
