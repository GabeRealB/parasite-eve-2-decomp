#include "common.h"

#include "actors/actor_401800.h"
#include "gameplay/1BC.h"

/// Per-frame blended animation tick: for pose slots 1..10 it sets both
/// contexts' slot rates (the blend context's from `field_8AA`, the pose
/// context's three below `field_8A2`), samples each context's pose and writes
/// their mix weighted by `field_8AC` against its 0x1000 complement. Slots 11
/// and up only take the pose rate and are advanced unblended.
void func_actor_401800_801337EC(Actor401800* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor401800AnimWork* work;

    work   = (Actor401800AnimWork*)arg0->field_1C;
    weight = work->field_8AC;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = (u8)work->field_8AA;
            work->slots[i].rate      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}
