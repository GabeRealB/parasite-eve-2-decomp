#include "common.h"

#include "actors/actor_401000.h"
#include "gameplay/1BC.h"

/// Ticks the body slots while a blend clip is active: slots 1-10 take the body
/// pose and the blend pose and write their mix, weighted by `field_8AC`, into
/// the model; slots 11-18 just advance the body clip. The body slots run three
/// sixteenths slower than `field_8A2`, the blend slots at `field_8AA`.
void func_actor_401000_80132A84(Actor401000* arg0)
{
    GpAnimPose       pose;
    GpAnimPose       blendPose;
    GpAnimCtx*       anim;
    s16              weight;
    s16              i;
    Actor401000Work* work;

    work   = arg0->field_1C;
    weight = work->field_8AC;
    anim   = &((Actor401000AnimWork*)work)->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            ((Actor401000AnimWork*)work)->blendSlots[i].rate = (u8)work->field_8AA;
            ((Actor401000AnimWork*)work)->slots[i].rate      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&((Actor401000AnimWork*)work)->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            ((Actor401000AnimWork*)work)->slots[i].rate = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&((Actor401000AnimWork*)work)->anim, i);
        }
    }
}
