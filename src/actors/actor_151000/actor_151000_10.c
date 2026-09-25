#include "common.h"

#include "actors/actor_151000.h"
#include "gameplay/1BC.h"

/// Ticks animation slots 1..0x12 of the enemy's animation context.
void func_actor_151000_801325C4(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_151000_8013D37C->anim, i);
        i++;
    } while (i < 0x13);
}
