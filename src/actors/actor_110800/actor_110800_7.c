#include "common.h"

#include "actors/actor_110800.h"

/// Sets the rate of animation slots 1..0x13 to 1 and resets each of them to
/// the current animation id, then records that id as the one now playing.
void func_actor_110800_80132424(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_110800_80139F10->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_110800_80139F10->anim, i, (s16)D_actor_110800_80139F10->animId);
        i++;
    } while (i < 0x14);
    D_actor_110800_80139F10->field_476 = D_actor_110800_80139F10->animId;
}
