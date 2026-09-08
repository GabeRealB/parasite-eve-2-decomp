#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105600_80131EAC;

INCLUDE_RODATA("actors/nonmatchings/actor_105600/actor_105600_3", D_actor_105600_80131EAC);

void func_actor_105600_801369CC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105600_80131EAC;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
