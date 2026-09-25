#include "common.h"

#include "actors/actor_143900.h"
#include "gameplay/1BC.h"

/// Ticks animation slots 1..0x13 of the first variant's animation context.
void func_actor_143900_801324C8(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_143900_801496B8->anim, i);
        i++;
    } while (i < 0x14);
}
