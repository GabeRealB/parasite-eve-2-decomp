/* Part of the scripted walk library; see scripted_walk.h. */

/// Restarts the selected walker's non-root tracks on its requested animation.
///
/// `SCRIPTED_WALK_WORK` must select a live work block with its context bound
/// to the twenty-slot rig, model coordinates and a loaded set containing tracks
/// 1 through 19 for `st.animId`. Each slot restarts at `ANIMATION_RATE_ONE`
/// without blending or writing a pose; root slot 0 is untouched. The applied
/// clip is recorded in `st.appliedAnimId`. Poses are applied on a later tick.
void SCRIPTED_WALK_RESET_ANIM(void)
{
    enum {
        SCRIPTED_WALK_FIRST_RESET_SLOT = 1,
        SCRIPTED_WALK_PRE_RESET_RATE   = 1,
    };
    s32 slotIndex;

    for (slotIndex = SCRIPTED_WALK_FIRST_RESET_SLOT; slotIndex < ARRAY_SIZE(SCRIPTED_WALK_WORK->rig.slots); slotIndex++) {
        // The reset replaces this preliminary sixteenth-frame rate with normal speed.
        SCRIPTED_WALK_WORK->rig.slots[slotIndex].rate = SCRIPTED_WALK_PRE_RESET_RATE;
        animationResetSlot(&SCRIPTED_WALK_WORK->rig.anim, slotIndex, SCRIPTED_WALK_WORK->st.animId);
    }
    SCRIPTED_WALK_WORK->st.appliedAnimId = SCRIPTED_WALK_WORK->st.animId;
}
