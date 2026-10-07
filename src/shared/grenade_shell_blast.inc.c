/* Part of the grenade shell library; see grenade_shell.h. */

/// Counts down the grenade's live blast sphere before advancing to teardown.
///
/// Entered in task state 2 with live `WeaponGrenadeWork`. Detonation has changed
/// the flight timer into a whole-frame countdown (8 normally, 1 for the airburst
/// round). Decrements before testing; zero or negative advances to state 3.
/// Collision bodies stay linked until the teardown handler runs.
static void _grenadeShellBlast(Task* task)
{
    enum { GRENADE_SHELL_TASK_STATE_EXIT = 3 };
    WeaponGrenadeWork* work  = task->work;
    s32                timer = work->flightTimer.word - 1;

    work->flightTimer.word = timer;
    if (timer <= 0) {
        task->state = GRENADE_SHELL_TASK_STATE_EXIT;
    }
}
