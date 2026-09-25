#include "common.h"

#include "actors/actor_151000.h"
#include "gameplay/1BC.h"

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a
/// reset argument, and latches the clip into `field_47E`. Clears the footstep
/// check's record first.
void func_actor_151000_80132610(void)
{
    s32 i;

    D_actor_151000_8013D37C->stepRec = NULL;
    i                                = 1;
    do {
        D_actor_151000_8013D37C->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_151000_8013D37C->anim, i, (s16)D_actor_151000_8013D37C->animId);
        i++;
    } while (i < 0x13);
    D_actor_151000_8013D37C->field_47E = D_actor_151000_8013D37C->animId;
}
