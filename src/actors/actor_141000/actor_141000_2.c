#include "common.h"

#include "actors/actor_141000.h"

#include <psyq/libgpu.h>
#include <psyq/libgs.h>

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80132E24);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80132EB0);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80132EF4);

void func_actor_141000_80132FC8(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80132FD0);

void func_actor_141000_8013308C(GsCOORDINATE2* arg0, s32 arg1)
{
    VECTOR scale;

    scale.vz = arg1;
    scale.vx = 0x1000;
    scale.vy = 0x1000;
    ScaleMatrix(&arg0->coord, &scale);
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801330C0);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801331AC);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80133204);

void func_actor_141000_80133260(Actor141000* arg0)
{
    Actor141000Point sp10[24];
    s32              spD0;
    s32              spD4;

    func_actor_141000_801323F0(arg0, sp10, &spD0, &spD4);
    func_actor_141000_80131E94(arg0, sp10, spD0);
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801332A0);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_80133490);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801335D4);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801336DC);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_801338C0);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_2", func_actor_141000_8013392C);
