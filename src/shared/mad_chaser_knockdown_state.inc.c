/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Combat state 9: runs the `subState` knockdown sub-state from a three-entry
/// table, then while `stateScratch` is 1 lets `_madChaserTakeKnockdownRequest` take
/// a pending request.
void madChaserKnockdownState(Task* arg0)
{
    MadChaserWork* work;
    TaskFuncTable3 sp;

    work = (MadChaserWork*)arg0->work;
    sp   = gMadChaserKnockdownSteps;
    sp.funcs[(s16)work->subState](arg0);
    if (work->stateScratch == 1) {
        _madChaserTakeKnockdownRequest(arg0);
    }
}
