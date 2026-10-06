/* Part of the pair walk library; see pair_walk.h. */

/// Advances the walker's eighteen animated parts, leaving root slot 0 alone.
///
/// Requires live `PairWalkWork` at `Task::work` and initialized slots 1 to 18.
/// The context borrows all nineteen model coordinates, loaded clip data and
/// pose-buffer entries; keep them live through playback. Rates are signed
/// sixteenths of a normal frame per tick. Uses animation's scratch stack and GTE.
static void _pairWalkTickAnim(Task* task)
{
    PairWalkWork* work;
    s32           slotIndex;

    work      = task->work;
    slotIndex = PAIR_WALK_FIRST_ANIM_SLOT;
    do {
        animationTickSlot(&work->rig.anim, slotIndex);
        slotIndex++;
    } while (slotIndex < (s32)ARRAY_SIZE(work->rig.slots));
}
