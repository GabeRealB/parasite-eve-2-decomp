#ifndef SRC_SHARED_ACTOR_MOTION_PLAY_HELPERS_H
#define SRC_SHARED_ACTOR_MOTION_PLAY_HELPERS_H

#include "actor_motion.h"

#include "gameplay/animation.h"

extern AnimationSet** gActorMotionAnimBanks[];

/// Restarts an actor's requested clip on slots 1..19 and enables frame ticking.
///
/// Requires writable work and a live twenty-part model. Before the first call,
/// set `work->model.bank` to `ACTOR_MODEL_STATE_NONE` and clear
/// `work->model.ticking`; later calls must use the same model and storage.
/// `source.index` and `animationId` must select loaded, non-NULL entries in
/// `gActorMotionAnimBanks` and fit 0..127: both narrow to signed bytes before
/// indexing. Clip zero is used unchanged. The request is read only through
/// this call and must not overlap playback storage; its collision choice is
/// ignored. Keep the work, model, bank tables and clip data live during playback.
///
/// A bank change binds the context. A nonzero blend choice on a ticking rig
/// advances and captures each old pose, then seeks its track start while
/// retaining the slot's rate and encoding. These slots must already be bound
/// to the requested bank, and the new clip must support their encoding.
/// `blendFrames` counts whole normal-rate frames; zero requests no transition
/// time, and 0..2047 keeps the narrowed remaining time nonnegative. Otherwise
/// slots reset at normal rate. Both paths tick the new pose before enabling
/// subsequent ticks, even for a repeated clip; slot 0 keeps the placed root.
/// Scratch-stack and GTE requirements follow `animationTickSlot`.
static inline void _actorMotionApplyAnimationRequest(ActorMotionPlayWork* work, TmdObject* model,
                                                     const AnimationPlayRequest* request)
{
    enum {
        ACTOR_MOTION_FIRST_DRIVEN_SLOT  = 1,
        ACTOR_MOTION_TRACK_START_OFFSET = 0,
    };
    s32 slotIndex;

    if (request->source.index != work->model.bank) {
        work->model.bank = request->source.index;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks[work->model.bank], model, work->rig.poses, work->rig.slots);
    }
    work->model.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        // Capture the old poses before any slot advances toward the new clip.
        for (slotIndex = ACTOR_MOTION_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId,
                                       ACTOR_MOTION_TRACK_START_OFFSET, request->blendFrames);
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
    work->model.ticking = true;
}

#endif
