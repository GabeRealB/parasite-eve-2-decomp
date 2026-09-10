#include "common.h"

#include "actors/actor_151000.h"

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
