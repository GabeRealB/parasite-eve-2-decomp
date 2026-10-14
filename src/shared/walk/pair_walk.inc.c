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

/// Captures the animated parts' current poses and blends toward the requested clip.
///
/// Requires live `PairWalkWork` at `Task::work`, initialized slots 1 to 18 and
/// loaded target tracks compatible with their current pose encodings. Seeks
/// each track's start, retaining its playback rate, and records the applied
/// clip; root slot 0 is untouched. `blendFrames` counts whole normal-rate frames
/// (0 to 2047 avoids signed blend-time overflow). The context's nineteen
/// coordinates, clip data and pose buffer must stay live through playback.
/// Uses animation's scratch stack and GTE, including a capture tick per slot.
static void _pairWalkReseedAnim(Task* task)
{
    PairWalkWork* work;
    s32           slotIndex;

    work      = task->work;
    slotIndex = PAIR_WALK_FIRST_ANIM_SLOT;
    do {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->st.animId, 0, work->blendFrames);
        slotIndex++;
    } while (slotIndex < (s32)ARRAY_SIZE(work->rig.slots));
    work->st.appliedAnimId = work->st.animId;
}
