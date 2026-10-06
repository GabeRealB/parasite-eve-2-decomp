/* Part of the scripted walk library; see scripted_walk.h. */

/// Blended reseed: reseeds animation slots 1..0x13 of the work block from the
/// current animation id with the latched reset argument
/// `gScriptedWalkBlendFrames`, and records that id as the one now playing.
void scriptedWalkBlendAnim(void)
{
    s32 i;

    i = 1;
    do {
        animationSeekSlotWithBlend(&SCRIPTED_WALK_WORK->rig.anim, i, SCRIPTED_WALK_WORK->st.animId, 0,
                                   gScriptedWalkBlendFrames);
        i++;
    } while (i < 0x14);
    SCRIPTED_WALK_WORK->st.appliedAnimId = SCRIPTED_WALK_WORK->st.animId;
}
