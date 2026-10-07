/* Part of the footstep walk library; see footstep_walk.h. */

/// Starts the published walker's requested animation synchronously.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` through `gFootstepWalkTask` and
/// `_gFootstepWalkWork`; the receiver argument, message ID and second payload
/// are ignored. A nonzero blend uses `request->blendFrames` in whole frames
/// (0..2047); reset ignores that duration. The request is borrowed only for
/// this call, but selected clips and rig storage must stay live for playback.
///
/// Returns -1 without changing state for IDs 35 and above, otherwise zero.
/// The upper-bound check alone does not validate negative IDs or unloaded
/// table entries; callers must select a loaded clip with tracks 1 through 18.
/// Bank selection and collision options in the request are ignored.
static s32 _footstepWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { FOOTSTEP_WALK_CLIP_ID_LIMIT = 35 };

    if (request->animationId < FOOTSTEP_WALK_CLIP_ID_LIMIT) {
        _gFootstepWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gFootstepWalkBlendFrames    = request->blendFrames;
        } else {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gFootstepWalkWork->st.field_6 = 0;
        _footstepWalkUpdate(gFootstepWalkTask);
        return 0;
    }
    return -1;
}
