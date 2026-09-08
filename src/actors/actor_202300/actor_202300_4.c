#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_202300_80149E98;

INCLUDE_RODATA("actors/nonmatchings/actor_202300/actor_202300_4", D_actor_202300_80149E98);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300_4", func_actor_202300_8014DBA8);

void func_actor_202300_8014DD08(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_202300_80149E98;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
