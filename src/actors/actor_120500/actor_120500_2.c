#include "common.h"

#include "actors/actor_120500.h"
#include "main/task.h"

void func_actor_120500_801328C0(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->field_1C;

    work->field_4B8 = arg0;
    work->field_4BA = 0;
}

void func_actor_120500_801328E0(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->field_1C;

    work->field_4C0 = arg0;
    work->field_4C2 = 0;
}

void func_actor_120500_80132900(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->field_1C;

    work->field_4C8 = arg0;
    work->field_4CA = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500_2", func_actor_120500_80132920);

INCLUDE_ASM("actors/nonmatchings/actor_120500/actor_120500_2", func_actor_120500_80132A04);
