/* Part of the actor motion library; see actor_motion.h. */

/// Binds the selected bank and starts a changed clip on a nineteen-part rig.
///
/// Has `_actorMotionPlayAnim19`'s work, model, request and borrowed-storage
/// requirements. An unchanged clip skips setup; driven slots exclude the root.
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
