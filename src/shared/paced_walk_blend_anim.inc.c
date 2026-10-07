/* Part of the paced walk library; see paced_walk.h. */

/// Starts a buffered blend to the requested clip on the walker's non-root parts.
///
/// `task->work` must hold the allocated `PACED_WALK_WORK_T`, whose `rig.anim`
/// is bound to its twenty slots, pose-buffer entries and model coordinates.
/// Slots 1 through 19 must have initialized playback. `st.animId` selects a
/// loaded clip, excluding `ANIMATION_SET_BUFFERED_POSE`, with tracks and pose
/// encoding compatible with each slot. Each destination starts at that slot's
/// track start, following any control records there.
///
/// Each slot ticks its existing playback once to capture the source pose;
/// skipped pose writes retain the previous buffer contents. Playback rates
/// and the capture tick's flags and boundary latch are retained. `blendFrames`
/// counts whole normal-rate frames: zero gives no transition time, and 0..2047
/// keeps the narrowed signed remaining time nonnegative. No bounds are checked.
/// Keep the work, coordinates and borrowed clip storage live through playback;
/// track/record bounds and scratch/GTE requirements follow
/// `animationSeekSlotWithBlend`. Slot 0 and `st.state` remain unchanged, while
/// `st.appliedAnimId` records the clip installed on the other slots.
static void PACED_WALK_BLEND_ANIM(Task* task)
{
    PACED_WALK_WORK_T* work;
    s32                slotIndex;

    work = task->work;
    // Walk and placement commands control the root (part 0) separately.
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->st.animId, 0, work->blendFrames);
    }
    work->st.appliedAnimId = work->st.animId;
}
