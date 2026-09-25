#include "common.h"

#include "actors/actor_420700.h"
#include "gameplay/1BC.h"

/// Sets the rate of animation slots 1..0x13 to 1 and resets each of them to
/// the current animation id, then records that id as the one now playing.
void func_actor_420700_80132538(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_420700_8013EFE0->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_420700_8013EFE0->anim, i, D_actor_420700_8013EFE0->field_4B8);
        i++;
    } while (i < 0x14);
    D_actor_420700_8013EFE0->field_4B6 = D_actor_420700_8013EFE0->field_4B8;
}
