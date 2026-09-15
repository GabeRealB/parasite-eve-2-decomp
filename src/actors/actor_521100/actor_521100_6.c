#include "common.h"

#include "actors/actor_521100.h"
#include "actors/actors_shared_801326b4.h"
#include "main/task.h"

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_521100_80136724(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickSlot(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3D8->slots[i]);
        i++;
    } while (i < 0x13);
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_6", func_actor_521100_8013677C);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_6", func_actor_521100_80136820);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_6", func_actor_521100_801368B0);

/// Animation-start handler: seeds the work block's `animId` with
/// `args->animId + 1`, rejecting anything whose incremented id is 0xB or up,
/// and leaves the actor in step 2 with `field_482` cleared before running the
/// step dispatcher.
///
/// The published task is read into a local between the first two stores on
/// purpose: that is where the original evaluates it, and it is what puts the
/// global's `lui`/`lw` ahead of the `li 2` and leaves the `field_482` clear
/// for the call's delay slot.
s32 func_actor_521100_801369B8(Task* task, s32 arg1, Actor521100AnimArgs* args)
{
    Task* dispatcher;

    if (args->animId + 1 < 0xB) {
        D_actor_521100_8016A3D8->animId    = (u16)args->animId + 1;
        dispatcher                         = ActorsShared801326b4Task;
        D_actor_521100_8016A3D8->field_47C = 2;
        D_actor_521100_8016A3D8->field_482 = 0;
        func_actor_521100_80135F2C(dispatcher);
        return 0;
    }
    return -1;
}
