#include "common.h"

#include "actors/actors_shared_80132180.h"

/// Marks animation slots 1..0x13 of the published work block as reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing. The two carriers of this body, `actor_110300` and
/// `actor_110800`, reach the same block through the global, so one object
/// serves both.
void ActorsShared80132180(void)
{
    s32 i;

    i = 1;
    do {
        ActorsShared80131f9cWork->slots[i].rate = 1;
        Gp_AnimResetSlot(&ActorsShared80131f9cWork->anim, i, (s16)ActorsShared80131f9cWork->animId);
        i++;
    } while (i < 0x14);
    ActorsShared80131f9cWork->field_476 = ActorsShared80131f9cWork->animId;
}
