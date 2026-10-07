/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Waits two updating death frames before the blast burst step.
///
/// Requires live Mad Chaser work in ordinary-death behavior 7, with stateFrames
/// reset on entry. The counter wraps as u16 and its low halfword is compared
/// signed; reaching two advances to behavior 8. Does not tick animation.
static void _madChaserDeathPause(Task* task)
{
    enum { MAD_CHASER_DEATH_BLAST_PAUSE_FRAMES = 2 };
    u16            elapsedFrames;
    MadChaserWork* work;

    work              = task->work;
    elapsedFrames     = work->stateFrames + 1;
    work->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames >= MAD_CHASER_DEATH_BLAST_PAUSE_FRAMES) {
        work->state = work->state + 1;
    }
}
