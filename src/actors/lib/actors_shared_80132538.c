#include "common.h"

#include "actors/actors_shared_80132538.h"

/// Marks animation slots 1..0x13 of the published work block as reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
void ActorsShared80132538(void)
{
    s32 i;

    i = 1;
    do {
        ActorsShared80131f9cWork->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&ActorsShared80131f9cWork->anim, i, ActorsShared80131f9cWork->field_4B8);
        i++;
    } while (i < 0x14);
    ActorsShared80131f9cWork->field_4B6 = ActorsShared80131f9cWork->field_4B8;
}
