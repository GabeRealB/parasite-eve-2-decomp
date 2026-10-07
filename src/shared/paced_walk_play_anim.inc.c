/* Part of the paced walk library; see paced_walk.h. */

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0x10 and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 pacedWalkPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    PacedWalkWork* work;

    work = task->work;
    if (args->animationId < 0x10) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = args->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        PACED_WALK_UPDATE(task);
        return 0;
    }
    return -1;
}
