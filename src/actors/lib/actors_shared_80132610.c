#include "common.h"

#include "actors/actors_shared_80132610.h"

/// Restarts animation slots 1..0x12 from `field_480`, flagging each slot's
/// `field_9` before the reset so it replays from the top.
void ActorsShared80132610(void)
{
    s32 i;

    ActorsShared80131f9cWork->field_4B8 = NULL;
    i                                   = 1;
    do {
        ActorsShared80131f9cWork->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&ActorsShared80131f9cWork->anim, i, (s16)ActorsShared80131f9cWork->field_480);
        i++;
    } while (i < 0x13);
    ActorsShared80131f9cWork->field_47E = ActorsShared80131f9cWork->field_480;
}
