#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"

/// Restarts the animation without a reset argument, the step routine's state
/// 2: marks animation slots 1..0x12 as reset-pending and reseeds each of them
/// from the current animation id, then records that id as the one now
/// playing. Reaches the block through `D_actor_451100_8014E744`.
void func_actor_451100_80132428(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_451100_8014E744->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_451100_8014E744->anim, i, (s16)D_actor_451100_8014E744->animId);
        i++;
    } while (i < 0x13);
    D_actor_451100_8014E744->field_47E = D_actor_451100_8014E744->animId;
}
