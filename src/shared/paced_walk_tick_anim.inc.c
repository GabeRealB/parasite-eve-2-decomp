/* Part of the paced walk library; see paced_walk.h. */

/// Advances the walker's animation slots 1 through 19 and applies their poses.
///
/// `task->work` must hold a live `PACED_WALK_WORK_T` with its rig context bound
/// to its twenty slots and pose entries. Slots 1 through 19 must already be
/// seeded with valid tracks and same-numbered model coordinates. Each uses
/// its stored signed rate in sixteenths of a frame, with the death-playback
/// adjustment of `animationTickSlot`. Slot 0 is untouched.
/// Keep the work block, model coordinates and borrowed clip data live through
/// playback. Track/record bounds and scratch/GTE requirements are those of
/// `animationTickSlot`.
static void PACED_WALK_TICK_ANIM(Task* task)
{
    PACED_WALK_WORK_T* work;
    s32                slotIndex;

    work = task->work;
    // The walk and placement commands control the root (part 0) separately.
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
}
