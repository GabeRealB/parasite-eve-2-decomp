#include "common.h"

#include "actors/actor_260500.h"

/// Ticks animation slots 1..0x12 of the work block's animation context.
void func_actor_260500_8014A568(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_260500_80159E4C->anim, i);
        i++;
    } while (i < 0x13);
}
