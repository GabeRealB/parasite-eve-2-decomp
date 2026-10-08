/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Dispatches one step of the scripted pull within combat behavior 10.
///
/// Requires live Mad Chaser work with subState in 0..5. Copies the complete six-
/// callback table before indexing as s16: start, reaction, resisting pull,
/// capture, limp pull and capture. The selected step owns its state changes;
/// the combat frame callback subsequently ticks animation and resolves collision.
static void _madChaserPullState(Task* task)
{
    MadChaserWork* work;
    TaskFuncTable6 steps;

    work  = task->work;
    steps = gMadChaserPullSteps;
    steps.funcs[(s16)work->subState](task);
}
