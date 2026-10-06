/* Part of the pair walk library; see pair_walk.h. */

/// Applies an indexed animation request immediately to the walker.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` with live `PairWalkWork` and a
/// borrowed, word-aligned request. The ID must select a loaded set below 6;
/// negative IDs and missing entries are not checked. Ignores the request's
/// source and collision words. Nonzero `blend` captures initialized slots and
/// keeps the low signed halfword of `blendFrames` (whole normal-rate frames;
/// 0 to 2047 avoids signed blend-time overflow); zero restarts without blending.
/// Consumes the request during dispatch and retains no payload pointer.
/// The message ID and second payload are ignored. Returns 0 on acceptance,
/// or -1 for an ID of 6 or above without changing playback state.
static s32 _pairWalkPlay(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    enum { PAIR_WALK_ANIMATION_ID_LIMIT = 6 };
    PairWalkWork* work;

    work = task->work;
    if (request->animationId < PAIR_WALK_ANIMATION_ID_LIMIT) {
        work->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = request->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        _pairWalkUpdate(task);
        return 0;
    }
    return -1;
}
