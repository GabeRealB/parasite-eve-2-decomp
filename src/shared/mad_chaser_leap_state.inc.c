/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Dispatches the leap's windup, lunge, turn, rebound and landing steps.
///
/// Requires live Mad Chaser work in combat behavior 4 with subState in 0..4.
/// Copies the carrier's five non-NULL void(Task*) callbacks before calling one;
/// the carrier must remain loaded. The combat frame owns animation, rotation
/// and collision updates after dispatch; this call retains all task storage.
static void _madChaserLeapState(Task* task)
{
    MadChaserWork* work;
    TaskFuncTable5 leapSteps;

    work      = task->work;
    leapSteps = gMadChaserLeapSteps;
    leapSteps.funcs[(s16)work->subState](task);
}
