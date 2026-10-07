/* Part of the scripted walk library; see scripted_walk.h. */

/// Starts a timed blend from the published walker's child-part poses to its requested clip.
///
/// `SCRIPTED_WALK_WORK` must select a live, initialized rig: slots 1 through 19
/// need valid playback endpoints, coordinates, tracks and encoded-pose buffers.
/// `st.animId` must select a loaded, non-NULL set supporting each slot's existing
/// track and encoding. Keep the work block, model, table and clip data live
/// through playback; capacities are not checked here.
///
/// `SCRIPTED_WALK_BLEND_FRAMES` counts whole normal-rate frames; zero requests no
/// transition time, and 0..2047 keeps the narrowed remaining time nonnegative.
/// Each seek ticks and captures the current pose before selecting the track's
/// start, retaining the slot's playback rate. Slot 0 retains the separately
/// placed root. Records `st.animId` in `st.appliedAnimId`; the caller advances
/// `st.state`. Scratch-stack and GTE requirements are those of `animationSeekSlotWithBlend`.
static void SCRIPTED_WALK_BLEND_ANIM(void)
{
    enum {
        SCRIPTED_WALK_FIRST_BLEND_SLOT   = 1,
        SCRIPTED_WALK_TRACK_START_OFFSET = 0,
    };
    s32 slotIndex;

    // Capture and retarget the child parts while leaving the placed root alone.
    for (slotIndex = SCRIPTED_WALK_FIRST_BLEND_SLOT; slotIndex < ARRAY_SIZE(SCRIPTED_WALK_WORK->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&SCRIPTED_WALK_WORK->rig.anim, slotIndex, SCRIPTED_WALK_WORK->st.animId,
                                   SCRIPTED_WALK_TRACK_START_OFFSET,
                                   SCRIPTED_WALK_BLEND_FRAMES);
    }
    SCRIPTED_WALK_WORK->st.appliedAnimId = SCRIPTED_WALK_WORK->st.animId;
}
