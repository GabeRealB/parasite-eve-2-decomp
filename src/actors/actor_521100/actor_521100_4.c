#include "common.h"

#include "actors/actor_521100.h"

void Gp_IncStateF0Ref(s32 arg0);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135D10);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135D58);

s32 func_actor_521100_80135D9C(Actor521100* arg0)
{
    arg0->field_1C->field_6B0 = 1;
    Gp_IncStateF0Ref(0);
    return 0;
}

s16 func_actor_521100_80135DC8(Actor521100* arg0)
{
    return arg0->field_1C->field_6B2;
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135DDC);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135F2C);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_801360C4);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80136290);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80136404);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80136604);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80136680);
