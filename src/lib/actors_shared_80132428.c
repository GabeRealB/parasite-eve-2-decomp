#include "common.h"

#include "actors/actors_shared_80132428.h"

/// The `ActorsShared801325c8` walk through the published global instead of
/// through `Task::work`: marks animation slots 1..0x12 as reset-pending and
/// reseeds each of them from the current animation set, then records that set
/// as the one now playing.
void ActorsShared80132428(void)
{
    s32 i;

    i = 1;
    do {
        ActorsShared80131f9cWork->slots[i].rate = 1;
        Gp_AnimResetSlot(&ActorsShared80131f9cWork->anim, i, ActorsShared80131f9cWork->animId);
        i++;
    } while (i < 0x13);
    ActorsShared80131f9cWork->field_47E = ActorsShared80131f9cWork->animId;
}
