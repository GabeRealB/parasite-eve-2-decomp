/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Advances the sub-state once the frame counter has passed 0x50.
void madChaserAlertWait(Task* arg0)
{
    u16            ticks;
    MadChaserWork* work;

    work              = (MadChaserWork*)arg0->work;
    ticks             = work->stateFrames;
    work->stateFrames = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->subState = work->subState + 1;
    }
}
