#include "common.h"

#include "actors/actor_110800.h"

/// Ticks animation slots 1..0x13 of the work block's animation context.
void func_actor_110800_801323DC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_110800_80139F10->anim, i);
        i++;
    } while (i < 0x14);
}
