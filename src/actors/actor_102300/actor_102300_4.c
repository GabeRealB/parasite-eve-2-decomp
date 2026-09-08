#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102300_80131E98;

INCLUDE_RODATA("actors/nonmatchings/actor_102300/actor_102300_4", D_actor_102300_80131E98);

INCLUDE_ASM("actors/nonmatchings/actor_102300/actor_102300_4", func_actor_102300_80135BA8);

void func_actor_102300_80135D08(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_102300_80131E98;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
