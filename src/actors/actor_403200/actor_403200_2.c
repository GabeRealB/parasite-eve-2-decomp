#include "common.h"
#include "actors/actor_403200.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_403200_80131E84;

extern s16 D_actor_403200_80141C5A;

extern Task* D_actor_403200_8015F8F0;

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141018);

void func_actor_403200_801410F0(s8 arg0)
{
    ((Actor403200Work*)D_actor_403200_8015F8F0->idMap)->field_EAC = arg0;
}

void func_actor_403200_80141108(s16 arg0)
{
    D_actor_403200_80141C5A = arg0;
}

s16 func_actor_403200_80141114(void)
{
    return D_actor_403200_80141C5A;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141124);

s16 func_actor_403200_80141180(Actor403200Obj* arg0, s16 arg1)
{
    return func_actor_403200_801344C4(arg0, arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_801411A8);

void func_actor_403200_8014122C(void)
{
}

void func_actor_403200_80141234(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_8014123C);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_801412D0);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_8014139C);

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

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_801414E8);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141564);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141670);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141778);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141800);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141868);

void func_actor_403200_8014196C(void)
{
}
