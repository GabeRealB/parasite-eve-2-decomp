/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Puts the task in state 5, the despawn phase, with `state` and
/// `subState` cleared.
void madChaserStartDespawn(Task* arg0)
{
    MadChaserWork* work;

    work           = (MadChaserWork*)arg0->work;
    arg0->state    = 5;
    work->state    = 0;
    work->subState = 0;
}
