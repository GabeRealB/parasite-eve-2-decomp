/* Part of the Mad Chaser library; see mad_chaser.h. */

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
    steps = gMadChaserLurkAlertSteps;
    if (_madChaserJoinAlert(task)) {
        work->busy = 0;
        return;
    }
    steps.funcs[(s16)work->subState](task);
}
