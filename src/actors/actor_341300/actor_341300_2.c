#include "common.h"

#include "main/task.h"

extern Task* D_actor_341300_80165A2C;

extern TaskDesc D_actor_341300_80165A68;

extern TaskDesc D_actor_341300_80165208;

extern Task* D_actor_341300_80165AA4;

extern s8 D_8007272D;

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_801623FC);

void func_actor_341300_8016241C(void)
{
    D_actor_341300_80165AA4 = Task_SpawnFromTable(&D_actor_341300_80165208, 0, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162450);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162478);

void func_actor_341300_80162530(void)
{
    D_actor_341300_80165AA4 = Task_SpawnFromTable(&D_actor_341300_80165208, 1, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162564);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162588);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_801625AC);

void func_actor_341300_80162680(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_341300_8016268C(void)
{
    D_actor_341300_80165AA4 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162698);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80162878);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80163028);

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_801631D4);

void func_actor_341300_8016398C(s32 arg0)
{
    if ((arg0 << 0x10) == 0) {
        D_actor_341300_80165A2C = Task_SpawnFromTable(&D_actor_341300_80165A68, 0, 0, 0);
    }
}

void func_actor_341300_801639CC(s32 arg0)
{
    if (((arg0 << 0x10) == 0) && (D_actor_341300_80165A2C != NULL)) {
        Task_Kill(D_actor_341300_80165A2C);
        D_actor_341300_80165A2C = NULL;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341300/actor_341300_2", func_actor_341300_80163A10);
