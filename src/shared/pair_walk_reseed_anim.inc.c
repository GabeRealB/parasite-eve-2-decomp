/* Part of the pair walk library; see pair_walk.h. */

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
