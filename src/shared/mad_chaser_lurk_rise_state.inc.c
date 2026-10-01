/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Lurk state 2: unless another Mad Chaser has raised the alert, runs its two sub-
/// states (request animation 0xF, then wait for it to end).
void madChaserLurkRiseState(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        madChaserLurkRiseStart,
        madChaserLurkRiseEnd,
    };

    if (madChaserJoinAlert(arg0) == 0) {
        states[(s16)work->field_422](arg0);
    }
}
