/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the next behavior state with its frame counter reset.
///
/// Requires a live Mad Chaser work block and a next entry in the active behavior
/// table. The 16-bit state increment wraps; the task state and sub-state are retained.
static void _madChaserAdvanceBehaviorState(Task* task)
{
    MadChaserWork* work;

    work              = task->work;
    work->stateFrames = 0;
    work->state       = work->state + 1;
}
