#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_444000_80131E90;

extern s16 D_actor_444000_80144A72;

/// The overlay's event/controller task, whose `idMap` holds an
/// `Actor444000EventWork`.
extern Actor444000* D_actor_444000_80161878;

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801433B8);

void func_actor_444000_80143490(s8 arg0)
{
    D_actor_444000_80161878->field_1C->field_EAC = arg0;
}

void func_actor_444000_801434A8(s16 arg0)
{
    D_actor_444000_80144A72 = arg0;
}

s16 func_actor_444000_801434B4(void)
{
    return D_actor_444000_80144A72;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801434C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801435CC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801436CC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80143798);

void func_actor_444000_8014382C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_444000_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_444000_80143888(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_444000_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_801438E4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80143960);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80143A6C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80143B74);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80143BFC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80143C64);

s32 func_actor_444000_80143D68(Actor444000* arg0)
{
    return arg0->field_20->field_40 > 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80143D7C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80143E68);

s32 func_actor_444000_80143F38(Actor444000* arg0)
{
    arg0->field_1C->field_0 = 0;
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80143F4C);
