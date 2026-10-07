/* Part of the actor motion library; see actor_motion.h. */

/// Starts a changed clip on the carrier's nineteen-part rig, preserving repeats.
///
/// Borrows writable `ActorMotion19PlayWork`, a live nineteen-part model and a
/// readable request that must not overlap playback storage. Bank and clip must
/// be loaded indices of `gActorMotionAnimBanks19`, representable as nonnegative
/// signed bytes. Initialize `model.bank` to `ACTOR_MODEL_STATE_NONE` before the
/// first request. A bank change binds the rig and invalidates its previous clip;
/// an unchanged clip in the same bank performs no playback work. Slots 1..18
/// blend for `blendFrames` whole frames (normally 0..2047) when requested and
/// already ticking, or reset otherwise, then tick once. Model coordinates,
/// work-owned slots/poses and clip data remain borrowed for playback's lifetime.
static inline void _actorMotionApplyAnimationRequest19(ActorMotion19PlayWork* work, TmdObject* model, const AnimationPlayRequest* request)
{
    enum { ACTOR_MOTION_FIRST_DRIVEN_SLOT = 1 };
    s32 slotIndex;

    // Changing banks invalidates the old clip even when the numeric ID agrees.
    if (request->source.index != work->model.bank) {
        work->model.bank   = request->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks19[work->model.bank], model, work->rig.poses, work->rig.slots);
    }
    if (request->animationId != work->model.animId) {
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
        // Apply the new pose immediately before normal frame ticking resumes.
        for (slotIndex = ACTOR_MOTION_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        work->model.ticking = 1;
    }
}

static s32 _actorMotionPlayAnim19(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    ActorMotion19PlayWork* work;
    TmdObject*             model;

    work  = task->work;
    model = task->extra.tmd;
    _actorMotionApplyAnimationRequest19(work, model, request);
    return 0;
}
