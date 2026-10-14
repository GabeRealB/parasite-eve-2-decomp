/* Part of the footstep walk library; see footstep_walk.h. */

/// Restarts parts 1 through 18 on the requested clip and rearms footstep cues.
///
/// The published work block must have its rig bound to live storage, model
/// coordinates and a loaded clip containing all driven tracks. Each slot
/// finishes at `ANIMATION_RATE_ONE`, with no pose advance or coordinate write;
/// part 0 is untouched. `st.appliedAnimId` records the selected clip.
static void _footstepWalkResetAnim(void)
{
    s32 slotIndex;

    _gFootstepWalkWork->stepRecord = NULL;
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(_gFootstepWalkWork->rig.slots); slotIndex++) {
        // The reset replaces this preliminary sixteenth-frame rate with normal speed.
        _gFootstepWalkWork->rig.slots[slotIndex].rate = FOOTSTEP_WALK_PRE_RESET_RATE;
        animationResetSlot(&_gFootstepWalkWork->rig.anim, slotIndex, _gFootstepWalkWork->st.animId);
    }
    _gFootstepWalkWork->st.appliedAnimId = _gFootstepWalkWork->st.animId;
}

/// Captures the current pose of parts 1 through 18 and blends to the requested clip.
///
/// `_gFootstepWalkWork` must name the live walker's bound rig with initialized
/// slots and loaded tracks for `st.animId`. `_gFootstepWalkBlendFrames` counts
/// whole frames (0..2047); pose-buffer and scratch/GTE requirements are those
/// of `animationSeekSlotWithBlend`. Each slot advances once to capture its
/// pose. Root part 0 stays under the placement and movement handlers' control.
/// Clearing the record latch lets a cue in the new clip sound again.
static void _footstepWalkBlendAnim(void)
{
    s32 slotIndex;

    _gFootstepWalkWork->stepRecord = NULL;
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(_gFootstepWalkWork->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&_gFootstepWalkWork->rig.anim, slotIndex, _gFootstepWalkWork->st.animId, 0,
                                   _gFootstepWalkBlendFrames);
    }
    _gFootstepWalkWork->st.appliedAnimId = _gFootstepWalkWork->st.animId;
}

/// Starts the published walker's requested animation synchronously.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` through `_gFootstepWalkTask` and
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
        _footstepWalkUpdate(_gFootstepWalkTask);
        return 0;
    }
    return -1;
}
