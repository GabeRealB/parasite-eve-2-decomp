#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_302600_80161E4C;
extern GpEnemyTaskFuncTable3 D_actor_302600_80161E58;

void func_actor_302600_80165BF0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_302600_80161E4C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_302600/actor_302600_5", func_actor_302600_80165C4C);

void func_actor_302600_80165DA0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_302600_80161E58;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
