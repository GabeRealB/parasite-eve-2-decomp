#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_202400_80149E24;

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400_3", func_actor_202400_8014C5B0);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400_3", func_actor_202400_8014C910);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400_3", func_actor_202400_8014CAC4);

void func_actor_202400_8014CBD0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_202400_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
