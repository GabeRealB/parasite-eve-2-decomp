#include "common.h"

#include "actors/actor_143900.h"

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_8013242C);

/// Ticks animation slots 1..0x13 of the actor's animation context.
void func_actor_143900_801324C8(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_143900_801496B8->anim, i);
        i++;
    } while (i < 0x14);
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_80132514);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_801325A4);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_80132624);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_801326B4);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_801326FC);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_80132778);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_8013279C);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_801328D4);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_80132A9C);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_80132DEC);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_2", func_actor_143900_80132E48);
