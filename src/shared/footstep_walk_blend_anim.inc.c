/* Part of the footstep walk library; see footstep_walk.h. */

/// Starts animation slots 1..0x12 on clip `animId`, forwarding
/// `gFootstepWalkBlendFrames` as the reset argument, and latches the clip into
/// `st.appliedAnimId`. Clears the footstep check's record first.
void footstepWalkBlendAnim(void)
{
    s32 i;

    gFootstepWalkWork->stepRecord = NULL;
    i                             = 1;
    do {
        animationSeekSlotWithBlend(&gFootstepWalkWork->rig.anim, i, gFootstepWalkWork->st.animId, 0,
                                   gFootstepWalkBlendFrames);
        i++;
    } while (i < 0x13);
    gFootstepWalkWork->st.appliedAnimId = gFootstepWalkWork->st.animId;
}
