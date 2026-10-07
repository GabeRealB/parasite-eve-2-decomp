#ifndef SRC_SHARED_ACTOR_MOTION_PLAY_HELPERS_H
#define SRC_SHARED_ACTOR_MOTION_PLAY_HELPERS_H

#include "actor_motion.h"

#include "gameplay/animation.h"

extern AnimationSet** gActorMotionAnimBanks[];

/// Applies an animation request to slots 1..19, including a repeated clip.
///
/// Borrows live twenty-part model coordinates, writable work and loaded bank
/// tables for the rig's lifetime. The request is borrowed through the call and
/// must not overlap playback storage. Its bank and clip must be loaded table
/// indices representable as nonnegative signed bytes. A bank change rebinds
/// the rig; a ticking rig blends for whole frames when requested, or resets.
/// The initial pose is ticked before enabling subsequent frame ticks.
static inline void _actorMotionApplyAnimationRequest(ActorMotionPlayWork* work, TmdObject* model,
                                                     const AnimationPlayRequest* request)
{
    enum { ACTOR_MOTION_FIRST_DRIVEN_SLOT = 1 };
    s32 slotIndex;

    if (request->source.index != work->model.bank) {
        work->model.bank = request->source.index;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks[work->model.bank], model, work->rig.poses, work->rig.slots);
    }
    work->model.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (slotIndex = ACTOR_MOTION_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
        }
    } else {
        for (slotIndex = ACTOR_MOTION_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
        }
    }
    // Seed the new pose before the actor's normal frame update can tick it.
    for (slotIndex = ACTOR_MOTION_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->model.ticking = 1;
}

#endif
