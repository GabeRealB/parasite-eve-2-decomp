/* Part of the paced walk library; see paced_walk.h. */

/// Advances the walker's nineteen articulated parts and applies their poses.
///
/// `task->work` must hold a live `PACED_WALK_WORK_T` with its rig context bound
/// to the twenty slots and pose entries. Slots 1 through 19 must be seeded
/// from loaded clips; the model coordinates and clip data stay live throughout
/// playback. Each call advances those slots at their stored rates and requires
/// the scratch/GTE state used by `animationTickSlot`.
void PACED_WALK_TICK_ANIM(Task* task)
{
    PACED_WALK_WORK_T* work;
    s32                slotIndex;

    work = task->work;
    // The walk and placement commands control the root (part 0) separately.
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
}
