#include "common.h"

#include "actors/actor_151000.h"

void ActorsShared8014a568(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&ActorsShared80131f9cWork->anim, i);
        i++;
    } while (i < 0x13);
}
