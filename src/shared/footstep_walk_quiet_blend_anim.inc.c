/* Part of the footstep walk library; see footstep_walk.h. */

/// Blended reseed: reseeds animation slots 1..0x12 of the work block from the
/// current animation id with the latched reset argument
/// `gFootstepWalkBlendFrames`, and records that id as the one now playing.
void footstepWalkQuietBlendAnim(void)
{
    s32 i;

    i = 1;
    do {
        animationSeekSlotWithBlend(&gFootstepWalkWork->rig.anim, i, gFootstepWalkWork->st.animId, 0,
                                   gFootstepWalkBlendFrames);
        i++;
    } while (i < 0x13);
    gFootstepWalkWork->st.appliedAnimId = gFootstepWalkWork->st.animId;
}
