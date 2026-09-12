#include "common.h"
#include "actors/actor_403200.h"
#include "gameplay/1BC.h"
#include "main/task.h"

extern s16 D_actor_403200_80141C5A;

extern Task* D_actor_403200_8015F8F0;

void func_actor_403200_801410F0(s8 arg0)
{
    ((Actor403200Work*)D_actor_403200_8015F8F0->idMap)->field_EAC = arg0;
}

void func_actor_403200_80141108(s16 arg0)
{
    D_actor_403200_80141C5A = arg0;
}

s16 func_actor_403200_80141114(void)
{
    return D_actor_403200_80141C5A;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141124);

s16 func_actor_403200_80141180(Actor403200Obj* arg0, s16 arg1)
{
    return func_actor_403200_801344C4(arg0, arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_801411A8);

void func_actor_403200_8014122C(void)
{
}

void func_actor_403200_80141234(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_8014123C);
