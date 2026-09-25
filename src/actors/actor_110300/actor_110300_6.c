#include "common.h"

#include "actors/actor_110300.h"
#include "gameplay/1BC.h"

/// Ticks animation slots 1..0x13 of the work block's animation context.
void func_actor_110300_80132138(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_110300_8013A0A0->anim, i);
        i++;
    } while (i < 0x14);
}
