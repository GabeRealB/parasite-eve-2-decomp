/* Part of the paced walk library; see paced_walk.h. */

/// Blends the walker's non-root parts toward the requested clip's track starts.
///
/// `task->work` holds a live `PACED_WALK_WORK_T`, with `rig.anim` bound to
/// its twenty slots, encoded-pose buffer and model coordinates. Slots 1 through
/// 19 must already be seeded. `st.animId` selects a loaded clip with valid
/// tracks compatible with those slots' existing encoding. Targets begin at
/// their track starts; control records may redirect them.
/// `blendFrames` counts whole normal-rate frames (0 requests no transition
/// time; 0..2047 keeps the signed remaining time nonnegative).
///
/// Each slot advances once before installing its buffered pose as the blend's
/// source. A skipped pose capture retains the buffer's existing contents.
/// Keep the work block, model coordinates and borrowed clip data live through
/// playback. Bounds, timing conversion and scratch/GTE requirements follow
/// `animationSeekSlotWithBlend`; none are checked here. Slot 0 and `st.state`
/// are unchanged; `st.appliedAnimId` records the requested clip.
void PACED_WALK_BLEND_ANIM(Task* task)
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
