/* Part of the pair walk library; see pair_walk.h. */

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 6 and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 pairWalkPlay(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    PairWalkWork* work;

    work = task->work;
    if (args->animationId < 6) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = args->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        pairWalkUpdate(task);
        return 0;
    }
    return -1;
}
