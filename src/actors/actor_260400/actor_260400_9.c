#include "common.h"

#include "actors/actor_260400.h"
#include "gameplay/1BC.h"

/// Plain reseed: marks animation slots 1..0x13 of the work block reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
void func_actor_260400_8014A7F8(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_260400_80154C70->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_260400_80154C70->anim, i, D_actor_260400_80154C70->field_4B8);
        i++;
    } while (i < 0x14);
    D_actor_260400_80154C70->field_4B6 = D_actor_260400_80154C70->field_4B8;
}
