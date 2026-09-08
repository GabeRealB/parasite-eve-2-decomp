#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105600_80131EA0;

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600_2", func_actor_105600_801367D0);

void func_actor_105600_80136888(void)
{
}

void func_actor_105600_80136890(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105600_80131EA0;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
