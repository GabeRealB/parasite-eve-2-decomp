/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Holds the alert until the previous frame count reaches 81.
///
/// Post-increments the unsigned halfword counter, testing its old value as s16.
/// Starting from zero takes 82 calls to advance the sub-state; the counter is
/// retained. Requires live Mad Chaser work.
static void _madChaserAlertWait(Task* task)
{
    enum {
        MAD_CHASER_ALERT_WAIT_THRESHOLD = 81,
    };
    u16            previousFrames;
    MadChaserWork* work;

    work              = task->work;
    previousFrames    = work->stateFrames;
    work->stateFrames = previousFrames + 1;
    if ((s16)previousFrames >= MAD_CHASER_ALERT_WAIT_THRESHOLD) {
        work->subState = work->subState + 1;
    }
}
