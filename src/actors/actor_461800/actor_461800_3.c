#include "common.h"

#include "actors/actor_461800.h"

/// Ticks animation slots 1..0x13 of the second variant's animation context.
void func_actor_461800_80133724(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_461800_801438A0->anim, i);
        i++;
    } while (i < 0x13);
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_3", func_actor_461800_80133770);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_3", func_actor_461800_8013380C);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_3", func_actor_461800_80133898);
