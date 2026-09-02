#include "common.h"

#include "actors/actor_120300.h"

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_2", func_actor_120300_80133E14);

void func_actor_120300_80133E34(s16 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->field_1C;

    work->field_4C8 = arg0;
    work->field_4CA = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_2", func_actor_120300_80133E54);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_2", func_actor_120300_80133E94);
