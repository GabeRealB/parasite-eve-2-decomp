/* Part of the footstep walk library; see footstep_walk.h. */

/// Captures parts 1 through 18 of the quiet walker and blends to its requested clip.
///
/// The published work block must have a bound rig, initialized slots and
/// loaded tracks for `st.animId`. `gFootstepWalkBlendFrames` counts whole
/// frames (0..2047). Each slot advances once during pose capture, with the
/// borrowed-buffer and scratch/GTE contract of `animationSeekSlotWithBlend`.
/// Part 0 is left alone; `st.appliedAnimId` records the newly selected clip.
static void _footstepWalkQuietBlendAnim(void)
{
    s32 slotIndex;

    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(gFootstepWalkWork->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&gFootstepWalkWork->rig.anim, slotIndex, gFootstepWalkWork->st.animId, 0,
                                   gFootstepWalkBlendFrames);
    }
    gFootstepWalkWork->st.appliedAnimId = gFootstepWalkWork->st.animId;
}
