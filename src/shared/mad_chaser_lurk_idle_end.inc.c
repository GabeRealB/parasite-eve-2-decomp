/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the lurk look behavior once the idle clip reports a boundary or jump.
///
/// Selects the look behavior at sub-state zero; the frame counter is retained
/// until the look hold begins. Requires the initialized Mad Chaser work.
static void _madChaserLurkIdleEnd(Task* task)
{
    MadChaserWork* work;
    s32            hasBoundaryStatus;

    work = task->work;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        hasBoundaryStatus = 1;
    } else {
        hasBoundaryStatus = 0;
    }
    if (hasBoundaryStatus) {
        work           = task->work;
        work->state    = MAD_CHASER_LURK_STATE_LOOK;
        work->subState = 0;
    }
}
