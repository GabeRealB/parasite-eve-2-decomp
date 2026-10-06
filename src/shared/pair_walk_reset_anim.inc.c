/* Part of the pair walk library; see pair_walk.h. */

/// Restarts the walker's animated parts on the requested clip without a pose blend.
///
/// Requires live `PairWalkWork` at `Task::work`, with its context bound to the
/// nineteen-part model and a loaded set selected by `st.animId`. Restarts slots
/// 1 to 18 on their own part tracks at `ANIMATION_RATE_ONE`, leaves root slot 0
/// alone and records the applied clip. Model poses are written on a later tick.
static void _pairWalkResetAnim(Task* task)
{
    enum { PAIR_WALK_PRE_RESET_RATE = 1 };
    PairWalkWork* work;
    s32           slotIndex;

    work = task->work;
    for (slotIndex = PAIR_WALK_FIRST_ANIM_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        // The restart replaces this preliminary rate with ANIMATION_RATE_ONE.
        work->rig.slots[slotIndex].rate = PAIR_WALK_PRE_RESET_RATE;
        animationResetSlot(&work->rig.anim, slotIndex, work->st.animId);
    }
    work->st.appliedAnimId = work->st.animId;
}
