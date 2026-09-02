#include "common.h"
#include "main/task.h"

extern s16      D_actor_136100_8013F17C;
extern TaskDesc ActorsShared80134898Desc;

void func_actor_136100_801348C8(void)
{
    Task_SpawnFromTable(&ActorsShared80134898Desc, 5, 9, 0);
}

void func_actor_136100_801348F8(void)
{
    D_actor_136100_8013F17C = 1;
    SetDispMask(1);
}

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100_2", func_actor_136100_80134924);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100_2", func_actor_136100_80134964);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100_2", func_actor_136100_801349B4);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100_2", func_actor_136100_80134A18);
