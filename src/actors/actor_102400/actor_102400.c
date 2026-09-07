#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102400_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80131E84);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801323DC);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_8013277C);

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80132A28);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80133240);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801333B0);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80133830);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801339B0);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80133D94);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80133EAC);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80134084);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801341D4);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80134318);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801345B0);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80134910);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80134AC4);

void func_actor_102400_80134BD0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_102400_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E5C);
