/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Sets the state machine to state 5, the alert, with `subState` cleared.
void madChaserToAlertState(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    work->state    = 5;
    work->subState = 0;
}
