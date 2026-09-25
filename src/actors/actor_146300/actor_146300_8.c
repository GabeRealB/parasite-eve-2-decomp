#include "common.h"

#include "actors/actor_146300.h"
#include "gameplay/1BC.h"

/// Ticks animation slots 1..0x13 of the work block's animation context.
void func_actor_146300_80132840(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_146300_80142828->anim, i);
        i++;
    } while (i < 0x14);
}
