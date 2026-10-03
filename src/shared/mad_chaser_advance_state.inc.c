/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Clears the frame counter `stateFrames` and advances `state`.
void madChaserAdvanceState(Task* arg0)
{
    MadChaserWork* work;

    work              = (MadChaserWork*)arg0->work;
    work->stateFrames = 0;
    work->state       = work->state + 1;
}
