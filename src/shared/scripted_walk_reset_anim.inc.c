/* Part of the scripted walk library; see scripted_walk.h. */

/// Plain reseed: marks animation slots 1..0x13 of the work block reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
void scriptedWalkResetAnim(void)
{
    s32 i;

    i = 1;
    do {
        gScriptedWalkWork->rig.slots[i].rate = 1;
        animationResetSlot(&gScriptedWalkWork->rig.anim, i, gScriptedWalkWork->st.animId);
        i++;
    } while (i < 0x14);
    gScriptedWalkWork->st.appliedAnimId = gScriptedWalkWork->st.animId;
}
