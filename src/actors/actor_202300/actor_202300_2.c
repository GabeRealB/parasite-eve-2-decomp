#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_202300_80149E80;

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300_2", func_actor_202300_8014D908);

void func_actor_202300_8014D9C0(void)
{
}

void func_actor_202300_8014D9C8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_202300_80149E80;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
