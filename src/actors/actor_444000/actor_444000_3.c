#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"
#include "main/task.h"

extern s16 D_actor_444000_80144A72;

/// The overlay's event/controller task, whose `idMap` holds an
/// `Actor444000EventWork`.
extern Actor444000* D_actor_444000_80161878;

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
