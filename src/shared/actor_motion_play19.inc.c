/* Part of the actor motion library; see actor_motion.h. */

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. An unchanged clip skips playback setup.
/// Blends an already ticking rig when requested, using a whole-frame duration;
/// otherwise resets the slots before ticking them.
s32 actorMotionPlayAnim19(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    ActorMotion19PlayWork* work;
    TmdObject*             ext;
    s32                    i;

    work = (ActorMotion19PlayWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks19[work->model.bank], ext, work->rig.poses, work->rig.slots);
    }
    if (msg->animationId != work->model.animId) {
        work->model.animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                animationResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}
