/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Lurk state 2: unless another Mad Chaser has raised the alert, runs its two sub-
/// states (request animation 0xF, then wait for it to end).
void madChaserLurkRiseState(Task* arg0)
{
    MadChaserWork* work                = (MadChaserWork*)arg0->work;
    void           (*states[2])(Task*) = {
        _madChaserLurkRiseStart,
        _madChaserLurkRiseEnd,
    };

    if (_madChaserJoinAlert(arg0) == 0) {
        states[(s16)work->subState](arg0);
    }
}
