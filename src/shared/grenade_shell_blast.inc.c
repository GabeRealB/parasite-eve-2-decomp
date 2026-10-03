/* Part of the grenade shell library; see grenade_shell.h. */

/// Task state 2, after detonation: keeps the widened blast sphere live for the
/// frames the detonation left in `flightTimer` (8, or 1 for attachment 0xB),
/// then moves to the exit state.
void grenadeShellBlast(Task* task)
{
    WeaponGrenadeWork* work  = task->work;
    s32                timer = work->flightTimer.word - 1;

    work->flightTimer.word = timer;
    if (timer <= 0) {
        task->state = 3;
    }
}
