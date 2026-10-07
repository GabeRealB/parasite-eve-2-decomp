/* Part of the footstep walk library; see footstep_walk.h. */

/// Restarts parts 1 through 18 of the quiet walker on its requested clip.
///
/// The published work block must have a bound rig and loaded tracks for
/// `st.animId`. Reset requires the live storage and indices described by
/// `animationResetSlot`; it leaves slots at `ANIMATION_RATE_ONE` without
/// advancing or writing a pose. Part 0 is untouched and `st.appliedAnimId`
/// records the selected clip.
static void _footstepWalkQuietResetAnim(void)
{
    s32 slotIndex;

    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(_gFootstepWalkWork->rig.slots); slotIndex++) {
        // The reset replaces this preliminary sixteenth-frame rate with normal speed.
        _gFootstepWalkWork->rig.slots[slotIndex].rate = FOOTSTEP_WALK_PRE_RESET_RATE;
        animationResetSlot(&_gFootstepWalkWork->rig.anim, slotIndex, _gFootstepWalkWork->st.animId);
    }
    _gFootstepWalkWork->st.appliedAnimId = _gFootstepWalkWork->st.animId;
}
