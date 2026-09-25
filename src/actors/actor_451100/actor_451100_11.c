#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"

void func_actor_451100_801323DC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_451100_8014E744->anim, i);
        i++;
    } while (i < 0x13);
}
