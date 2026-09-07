#include "common.h"

#include "actors/actor_151000.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_801324FC);

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_151000_801325C4(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&ActorsShared80131f9cWork->anim, i);
        i++;
    } while (i < 0x13);
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_80132610);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_801326AC);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_80132738);
