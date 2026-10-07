/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Returns 1 when slot 1 reports an animation boundary, control jump, or held boundary.
///
/// Reads the latest tick/walk status without consuming it. A looping clip can
/// report a jump while continuing to play; this is also the walk's footstep gate.
/// Requires the task's initialized Mad Chaser animation storage.
static s16 _madChaserAnimHasBoundaryStatus(Task* task)
{
    MadChaserWork* work = task->work;

    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}
