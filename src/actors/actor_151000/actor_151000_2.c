#include "common.h"

#include "actors/actor_151000.h"

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_2", func_actor_151000_801324FC);

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_151000_801325C4(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_151000_8013D37C->anim, i);
        i++;
    } while (i < 0x13);
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_2", func_actor_151000_80132610);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_2", func_actor_151000_801326AC);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_2", func_actor_151000_80132738);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_2", func_actor_151000_801327C8);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_2", func_actor_151000_80132810);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_2", func_actor_151000_8013288C);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_2", func_actor_151000_801328DC);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_2", func_actor_151000_80132A38);
