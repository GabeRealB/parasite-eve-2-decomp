#include "common.h"

#include "actors/actor_260400.h"
#include "gameplay/1BC.h"

/// Ticks animation slots 1..0x13 of the work block's animation context.
void func_actor_260400_8014A7AC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_260400_80154C70->anim, i);
        i++;
    } while (i < 0x14);
}
