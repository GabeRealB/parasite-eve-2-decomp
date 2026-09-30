/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Lurk state 2: unless another hopper has raised the alert, runs its two sub-
/// states (request animation 0xF, then wait for it to end).
void hopperLurkRiseState(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->work;
    void             (*states[2])(Task*) = {
        hopperLurkRiseStart,
        hopperLurkRiseEnd,
    };

    if (hopperJoinAlert(arg0) == 0) {
        states[(s16)work->field_422](arg0);
    }
}
