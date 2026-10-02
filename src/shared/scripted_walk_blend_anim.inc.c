/* Part of the scripted walk library; see scripted_walk.h. */

/// Blended reseed: reseeds animation slots 1..0x13 of the work block from the
/// current animation id with the latched reset argument
/// `gScriptedWalkBlendFrames`, and records that id as the one now playing.
void scriptedWalkBlendAnim(void)
{
    s32 i;

    i = 1;
    do {
        animationSeekSlotWithBlend(&gScriptedWalkWork->rig.anim, i, gScriptedWalkWork->st.animId, 0,
                                   gScriptedWalkBlendFrames);
        i++;
    } while (i < 0x14);
    gScriptedWalkWork->st.appliedAnimId = gScriptedWalkWork->st.animId;
}
