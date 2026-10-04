/* Part of the actor motion library; see actor_motion.h. */

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. The requested clip is applied to the slots.
/// Blends an already ticking rig when requested, using a whole-frame duration;
/// otherwise resets the slots before ticking them.
s32 actorMotionPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    ActorMotionPlayWork* work;
    TmdObject*           ext;
    s32                  i;

    work = (ActorMotionPlayWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank = msg->source.index;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks[work->model.bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    work->model.animId = msg->animationId;
    if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            animationResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x14; i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}
