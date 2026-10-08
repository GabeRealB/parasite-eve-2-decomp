/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Dispatches the current knockdown step, then accepts hits for an upright stance.
///
/// Requires live Mad Chaser work in combat behavior 9, with subState in 0..2.
/// Copies the complete three-callback table before indexing as s16. After the
/// step, saved stance 1 permits a pending status or knockdown reaction to replace
/// the behavior, even if recovery just changed it. Other stances do not consume
/// requests here. Animation playback belongs to the combat frame callback.
static void _madChaserKnockdownState(Task* task)
{
    MadChaserWork* work;
    TaskFuncTable3 steps;

    work  = task->work;
    steps = gMadChaserKnockdownSteps;
    steps.funcs[(s16)work->subState](task);
    // The dispatched step may have selected a new behavior; still process this hit.
    if (work->stateScratch == MAD_CHASER_STANCE_UPRIGHT) {
        _madChaserTakeKnockdownRequest(task);
    }
}
