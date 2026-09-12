#include "common.h"
#include "actors/actor_403200.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_403200_80131E84;

void func_actor_403200_80141430(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E84;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403200_8014148C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E84;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_801414E8);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80141564);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80141670);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80141778);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80141800);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80141868);

void func_actor_403200_8014196C(void)
{
}
