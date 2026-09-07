#include "common.h"

#include "actors/actor_143900.h"

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_3", func_actor_143900_8013242C);

/// Ticks animation slots 1..0x13 of the actor's animation context.
void func_actor_143900_801324C8(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&ActorsShared80131f9cWork->anim, i);
        i++;
    } while (i < 0x14);
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_3", func_actor_143900_80132514);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_3", func_actor_143900_801325A4);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_3", func_actor_143900_80132624);
