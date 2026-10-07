/* Part of the footstep walk library; see footstep_walk.h. */

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
