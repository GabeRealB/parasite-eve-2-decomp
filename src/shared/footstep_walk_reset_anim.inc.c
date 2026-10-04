/* Part of the footstep walk library; see footstep_walk.h. */

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a
/// reset argument, and latches the clip into `st.appliedAnimId`. Clears the footstep
/// check's record first.
void footstepWalkResetAnim(void)
{
    s32 i;

    gFootstepWalkWork->stepRecord = NULL;
    i                             = 1;
    do {
        gFootstepWalkWork->rig.slots[i].rate = 1;
        animationResetSlot(&gFootstepWalkWork->rig.anim, i, gFootstepWalkWork->st.animId);
        i++;
    } while (i < 0x13);
    gFootstepWalkWork->st.appliedAnimId = gFootstepWalkWork->st.animId;
}
