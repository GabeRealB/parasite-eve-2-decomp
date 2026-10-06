/* Part of the scripted walk library; see scripted_walk.h. */

/// Restarts slots 1 through 19 on the published walker's requested clip.
///
/// `SCRIPTED_WALK_WORK` must select a non-NULL, live block whose context is
/// bound to its rig's slots and the model's part coordinates. `st.animId`
/// must be a nonnegative, in-range index selecting a non-NULL loaded set,
/// with tracks and coordinates for every driven slot. Keep the block, model,
/// table and clip data live through playback; capacities are not checked here.
///
/// Each slot restarts without a blend at `ANIMATION_RATE_ONE` (16 sixteenth-frame
/// units per tick), replacing its previous rate. Slot 0 and the current model
/// pose are untouched; a later tick applies the restarted tracks. Records the requested
/// clip in `st.appliedAnimId` and leaves `st.state` for the caller to advance.
static void SCRIPTED_WALK_RESET_ANIM(void)
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
