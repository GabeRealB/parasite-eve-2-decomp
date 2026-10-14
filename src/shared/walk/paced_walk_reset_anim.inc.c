/* Part of the paced walk library; see paced_walk.h. */

/// Restarts the requested clip on the walker's non-root model parts without blending.
///
/// `task->work` holds a live `PACED_WALK_WORK_T`, with `rig.anim` bound to
/// `rig.slots` and the model coordinates. `st.animId` selects a loaded clip
/// with valid tracks for parts 1 through 19. The context's borrowed set table,
/// clip data and coordinates must remain live while those slots play; bounds
/// follow `animationResetSlot` and are not checked here.
///
/// Each slot is primed at its track start with normal rate (`ANIMATION_RATE_ONE`)
/// and cleared boundary state. The next animation tick applies the pose.
/// Slot 0 and `st.state` are unchanged; `st.appliedAnimId` records the clip
/// installed on the other slots.
static void PACED_WALK_RESET_ANIM(Task* task)
{
    // Preliminary rate in sixteenths of a frame, overwritten by the slot reset.
    enum { PACED_WALK_RESET_PRESET_RATE = 1 };

    PACED_WALK_WORK_T* work;
    s32                slotIndex;

    work = task->work;
    // Walk and placement commands control the root (part 0) separately.
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = PACED_WALK_RESET_PRESET_RATE;
        animationResetSlot(&work->rig.anim, slotIndex, work->st.animId);
    }
    work->st.appliedAnimId = work->st.animId;
}
