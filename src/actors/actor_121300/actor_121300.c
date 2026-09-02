#include "common.h"

#include "main/task.h"

extern s16 D_800691CA;

extern TaskDesc ActorsShared80136280Desc;

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80131EB0);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_801326EC);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80132818);

INCLUDE_RODATA("actors/nonmatchings/actor_121300/actor_121300", D_actor_121300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013293C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133064);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013322C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013343C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133580);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133730);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133854);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133BFC);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80133D98);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013400C);

void func_actor_121300_801340F0(void)
{
    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
}

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013411C);

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_801341A8);

void func_actor_121300_80134224(s32 arg0, s32 arg1, s32 arg2)
{
    Task_SpawnFromTable(&ActorsShared80136280Desc, 0xA, arg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_80134250);

void func_actor_121300_80134270(void)
{
    D_800691CA = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_121300/actor_121300", func_actor_121300_8013427C);

void func_actor_121300_801342D4(s32 arg0)
{
    Task_SpawnFromTable(&ActorsShared80136280Desc, 1, arg0, 0);
}
