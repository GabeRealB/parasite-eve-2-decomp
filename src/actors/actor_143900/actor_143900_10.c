#include "common.h"

#include "actors/actor_143900.h"
#include "gameplay/1BC.h"

/// Marks animation slots 1..0x13 of the first variant's work block
/// reset-pending and reseeds each of them from the current animation id, then
/// records that id as the one now playing.
void func_actor_143900_80132514(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_143900_801496B8->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_143900_801496B8->anim, i, D_actor_143900_801496B8->field_4B8);
        i++;
    } while (i < 0x14);
    D_actor_143900_801496B8->field_4B6 = D_actor_143900_801496B8->field_4B8;
}
