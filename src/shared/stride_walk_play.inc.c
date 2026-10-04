/* Part of the stride walk library; see stride_walk.h. */

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0xC and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 strideWalkPlay(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    StrideWalkWork* work;

    work = task->work;
    if (args->animationId < 0xC) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = args->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        strideWalkUpdate(task);
        return 0;
    }
    return -1;
}
