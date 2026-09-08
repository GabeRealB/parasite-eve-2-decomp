#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_205600_80149EAC;

INCLUDE_RODATA("actors/nonmatchings/actor_205600/actor_205600_3", D_actor_205600_80149EAC);

void func_actor_205600_8014E9CC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_205600_80149EAC;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
