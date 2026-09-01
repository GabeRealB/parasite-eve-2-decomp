#include "common.h"

#include "main/task.h"

extern TaskDesc D_actor_150400_80132CF0;
extern Task*    D_actor_150400_8013C924;
extern Task*    D_actor_150400_8013C928;

INCLUDE_ASM("actors/nonmatchings/actor_150400/actor_150400", func_actor_150400_80131ECC);

INCLUDE_ASM("actors/nonmatchings/actor_150400/actor_150400", func_actor_150400_80131F6C);

void func_actor_150400_80131F9C(s32 arg0)
{
    D_actor_150400_8013C924->state = arg0;
    D_actor_150400_8013C928->state = arg0;
}

void func_actor_150400_80131FB8(void)
{
    D_actor_150400_8013C924 = Task_SpawnFromTable(&D_actor_150400_80132CF0, 0, 1, 0);
    D_actor_150400_8013C928 = Task_SpawnFromTable(&D_actor_150400_80132CF0, 0, 2, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_150400/actor_150400", func_actor_150400_80132014);

INCLUDE_ASM("actors/nonmatchings/actor_150400/actor_150400", func_actor_150400_80132228);

INCLUDE_ASM("actors/nonmatchings/actor_150400/actor_150400", func_actor_150400_801323E0);

INCLUDE_ASM("actors/nonmatchings/actor_150400/actor_150400", func_actor_150400_80132434);

INCLUDE_RODATA("actors/nonmatchings/actor_150400/actor_150400", D_actor_150400_80131E20);
