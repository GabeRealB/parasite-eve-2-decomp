#include "common.h"

#include "actors/actor_521100.h"

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_521100_80136724(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickSlot(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3D8->slots[i]);
        i++;
    } while (i < 0x13);
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_6", func_actor_521100_8013677C);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_6", func_actor_521100_80136820);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_6", func_actor_521100_801368B0);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_6", func_actor_521100_801369B8);
