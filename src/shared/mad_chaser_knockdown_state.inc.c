/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Combat state 9: runs the `field_422` knockdown sub-state from a three-entry
/// table, then while `field_44F` is 1 lets `madChaserTakeKnockdownRequest` take
/// a pending request.
void madChaserKnockdownState(Task* arg0)
{
    MadChaserWork* work;
    TaskFuncTable3 sp;

    work = (MadChaserWork*)arg0->work;
    sp   = gMadChaserKnockdownSteps;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        madChaserTakeKnockdownRequest(arg0);
    }
}
