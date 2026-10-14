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

/// Runs the lurk alert sequence until a claimed shared alert admits it to combat.
///
/// Requires live work and subState in 0..2. Copies the three lurk-alert steps
/// before checking the shared alert. Joining it enters the combat alert at
/// sub-state zero, clears busy, and skips this frame's step; otherwise exactly
/// one selected step runs and owns its state changes.
static void _madChaserLurkAlertState(Task* task)
{
    MadChaserWork* work;
    TaskFuncTable3 steps;

    work  = task->work;
    steps = _gMadChaserLurkAlertSteps;
    if (_madChaserJoinAlert(task)) {
        work->busy = 0;
        return;
    }
    steps.funcs[(s16)work->subState](task);
}
