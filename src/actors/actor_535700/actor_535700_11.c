#include "common.h"

#include "actors/actor_535700.h"
#include "gameplay/1BC.h"

/// Ticks animation slots 1..0x12 of the first enemy's animation context.
void func_actor_535700_80132648(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_535700_80146844->anim, i);
        i++;
    } while (i < 0x13);
}
