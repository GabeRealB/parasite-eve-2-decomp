#include "common.h"

#include "actors/actor_535700.h"
#include "gameplay/1BC.h"

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a reset
/// argument, and latches the clip into `field_47E`. Clears the footstep
/// check's record first.
void func_actor_535700_80132694(void)
{
    s32 i;

    D_actor_535700_80146844->field_4B8 = NULL;
    i                                  = 1;
    do {
        D_actor_535700_80146844->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_535700_80146844->anim, i, (s16)D_actor_535700_80146844->animId);
        i++;
    } while (i < 0x13);
    D_actor_535700_80146844->field_47E = D_actor_535700_80146844->animId;
}
