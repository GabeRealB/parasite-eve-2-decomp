/* Part of the stride walk library; see stride_walk.h. */

/// Immediately reseeds the walker's non-root tracks from a scripted play request.
///
/// Requires the spawned TMD walker and live `StrideWalkWork` rig. `request`
/// is borrowed only for this call. Clip IDs 1..11 select loaded sets; slot 0
/// of the bank is NULL. The retained check rejects only IDs >=12, so callers
/// must also exclude zero and negative IDs. Reset restarts the tracks; nonzero
/// `blend` captures their old poses and takes `blendFrames`' low signed
/// halfword as the duration in whole normal-rate frames; 0..2047 keeps the
/// slots' signed transition time nonnegative. Travel is unchanged.
/// Ignores source, collision choice, message ID and second payload. Returns
/// 0 after reseeding or -1 for an out-of-bank ID, leaving state unchanged.
static s32 _strideWalkPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 secondArg)
{
    enum {
        STRIDE_WALK_ANIMATION_ID_LIMIT = 12,
        STRIDE_WALK_PLAY_ACCEPTED      = 0,
        STRIDE_WALK_PLAY_REJECTED      = -1,
    };
    StrideWalkWork* work;

    work = task->work;
    if (request->animationId < STRIDE_WALK_ANIMATION_ID_LIMIT) {
        work->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = request->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        _strideWalkUpdate(task);
        return STRIDE_WALK_PLAY_ACCEPTED;
    }
    return STRIDE_WALK_PLAY_REJECTED;
}
