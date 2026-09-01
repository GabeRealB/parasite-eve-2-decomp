#include "common.h"
#include "main/task.h"

extern TaskDesc D_actor_120300_80141B6C;

void func_actor_120300_80133DF4(void)
{
    Gp_RestoreStreamRng();
}

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_2", func_actor_120300_80133E14);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_2", func_actor_120300_80133E34);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_2", func_actor_120300_80133E54);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_2", func_actor_120300_80133E94);

void func_actor_120300_80133EE4(void)
{
    Task_SpawnFromTable(&D_actor_120300_80141B6C, 4, 9, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_2", func_actor_120300_80133F14);
