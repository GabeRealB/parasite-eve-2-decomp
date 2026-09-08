#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105700_80131EA0;

INCLUDE_ASM("actors/nonmatchings/actor_105700/actor_105700_2", func_actor_105700_80136DA0);

void func_actor_105700_80136E58(void)
{
}

void func_actor_105700_80136E60(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105700_80131EA0;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
