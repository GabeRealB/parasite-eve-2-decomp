/* Part of the paced walk library; see paced_walk.h. */

/// Restarts the walker's slots 1 through 19 on the requested clip without blending.
///
/// `task->work` must hold a live `PACED_WALK_WORK_T` whose rig context is
/// bound to its twenty slots and the model's same-numbered coordinates.
/// `st.animId` must select a loaded clip with tracks for those parts; bounds
/// and borrowed-data lifetimes follow `animationResetSlot`. No bounds are
/// checked here. Each reset installs normal rate (`ANIMATION_RATE_ONE`) and
/// primes the track for the next tick; no pose is applied or captured here.
/// Slot 0 is untouched. Records the clip in `st.appliedAnimId`; the caller
/// advances `st.state` after the reset.
void PACED_WALK_RESET_ANIM(Task* task)
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
