#include "common.h"

#include "actors/actor_420700.h"
#include "gameplay/1BC.h"

/// Advances animation slots 1..0x13 of the work block by one tick.
void func_actor_420700_801324EC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_420700_8013EFE0->anim, i);
        i++;
    } while (i < 0x14);
}
