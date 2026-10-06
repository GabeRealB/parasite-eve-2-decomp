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

    gFootstepWalkWork->stepRecord = NULL;
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(gFootstepWalkWork->rig.slots); slotIndex++) {
        // The reset replaces this preliminary sixteenth-frame rate with normal speed.
        gFootstepWalkWork->rig.slots[slotIndex].rate = FOOTSTEP_WALK_PRE_RESET_RATE;
        animationResetSlot(&gFootstepWalkWork->rig.anim, slotIndex, gFootstepWalkWork->st.animId);
    }
    gFootstepWalkWork->st.appliedAnimId = gFootstepWalkWork->st.animId;
}
