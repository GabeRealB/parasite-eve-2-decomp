/* Part of the scripted walk library; see scripted_walk.h. */

/// Plain reseed: marks animation slots 1..0x13 of the work block reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
void scriptedWalkResetAnim(void)
{
    s32 i;

    i = 1;
    do {
        SCRIPTED_WALK_WORK->rig.slots[i].rate = 1;
        animationResetSlot(&SCRIPTED_WALK_WORK->rig.anim, i, SCRIPTED_WALK_WORK->st.animId);
        i++;
    } while (i < 0x14);
    SCRIPTED_WALK_WORK->st.appliedAnimId = SCRIPTED_WALK_WORK->st.animId;
}
