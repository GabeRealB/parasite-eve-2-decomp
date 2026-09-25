#include "common.h"

#include "actors/actor_260500.h"

/// Plain reseed: marks animation slots 1..0x12 of the work block reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
void func_actor_260500_8014A5B4(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_260500_80159E4C->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_260500_80159E4C->anim, i, D_actor_260500_80159E4C->field_480);
        i++;
    } while (i < 0x13);
    D_actor_260500_80159E4C->field_47E = D_actor_260500_80159E4C->field_480;
}
