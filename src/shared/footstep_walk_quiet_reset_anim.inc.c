/* Part of the footstep walk library; see footstep_walk.h. */

/// Plain reseed: marks animation slots 1..0x12 of the work block reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
void footstepWalkQuietResetAnim(void)
{
    s32 i;

    i = 1;
    do {
        gFootstepWalkWork->rig.slots[i].rate = 1;
        animationResetSlot(&gFootstepWalkWork->rig.anim, i, gFootstepWalkWork->st.animId);
        i++;
    } while (i < 0x13);
    gFootstepWalkWork->st.appliedAnimId = gFootstepWalkWork->st.animId;
}
