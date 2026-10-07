/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Dispatches the lurk rise unless a claimed alert switches the enemy to combat.
///
/// Requires live Mad Chaser work in lurk behavior 2 and subState in 0..1.
/// Sub-state 0 requests the rise clip; 1 waits for slot 1's boundary/jump/held
/// status before returning to lurk idle. Any claimed Mad Chaser alert, including
/// this enemy's, instead selects combat alert at sub-state zero. The lurk frame
/// callback owns animation ticking and root updates.
static void _madChaserLurkRiseState(Task* task)
{
    MadChaserWork* work        = task->work;
    TaskFunc       riseSteps[] = {
        _madChaserLurkRiseStart,
        _madChaserLurkRiseEnd,
    };

    if (_madChaserJoinAlert(task) == 0) {
        riseSteps[(s16)work->subState](task);
    }
}
