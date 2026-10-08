/* Part of the Mad Chaser library; see mad_chaser.h. */

// MAD_CHASER_FOUR_STEP_STATE selects a previously declared void(Task*) callback;
// that declaration establishes linkage. The default is madChaserDangleState.
// The carrier supplies gMadChaserDangleSteps as a complete TaskFuncTable4.
// The identifier binding affects only this definition, performs no argument
// evaluation or token construction, and is undefined after the inclusion.
#ifndef MAD_CHASER_FOUR_STEP_STATE
#define MAD_CHASER_FOUR_STEP_STATE madChaserDangleState
#endif

/// Dispatches one phase of the dangle or lurk-shift behavior.
///
/// Borrows the task's live MadChaserWork; subState must be in 0..3 and the
/// selected callback must be loaded. The dangle phases are start, sway, fall
/// and land; the lurk-shift phases are opening clip, start right step, move
/// right and return left. Each call copies the complete four-handler table
/// before reading the signed-halfword index, then calls exactly one handler.
void MAD_CHASER_FOUR_STEP_STATE(Task* task)
{
    MadChaserWork* work         = task->work;
    TaskFuncTable4 stepHandlers = gMadChaserDangleSteps;

    stepHandlers.funcs[(s16)work->subState](task);
}

#undef MAD_CHASER_FOUR_STEP_STATE
