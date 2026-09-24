#include "common.h"

#include "actors/actor_101900.h"
#include "gameplay/1BC.h"

/// Advances the actor's animation one tick. Joints 1-10 are sampled from both
/// the main and the blend animation and passed to `Gp_AnimWritePoseCopy` with
/// weights `field_8AC` and 0x1000 - `field_8AC`; joints 11-18 tick the main
/// animation alone. The per-joint rates come from `field_8A2` and `field_8AA`.
void Actor01900_Fn01950(Actor01900* arg0)
{
    GpAnimPose          pose;
    GpAnimPose          blendPose;
    GpAnimCtx*          anim;
    s16                 weight;
    s16                 i;
    Actor01900AnimWork* work;

    work   = (Actor01900AnimWork*)arg0->field_1C;
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
