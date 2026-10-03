/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hit flags are set, moves the state machine to state 3 (when
/// `stateScratch` is 1) or 5.
void madChaserKnockdownEnd(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (madChaserAnimEnded(arg0)) {
        if (work->stateScratch == 1) {
            MadChaserWork* w = (MadChaserWork*)arg0->work;

            w->state    = 3;
            w->subState = 0;
        } else {
            MadChaserWork* w = (MadChaserWork*)arg0->work;

            w->state    = 5;
            w->subState = 0;
        }
    }
}
