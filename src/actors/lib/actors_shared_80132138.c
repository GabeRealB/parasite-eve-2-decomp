#include "common.h"

#include "actors/actors_shared_80132138.h"

/// Ticks animation slots 1..0x13 of the published work block's animation
/// context. The two carriers of this body, `actor_110300` and `actor_110800`,
/// reach the same block through the global, so one object serves both.
void ActorsShared80132138(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&ActorsShared80131f9cWork->anim, i);
        i++;
    } while (i < 0x14);
}
