/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Combat state 10: runs the `field_422` sub-state from the six-entry pull
/// table.
void madChaserPullState(Task* arg0)
{
    MadChaserWork* work;
    TaskFuncTable6 sp;

    work = (MadChaserWork*)arg0->work;
    sp   = gMadChaserPullSteps;
    sp.funcs[(s16)work->field_422](arg0);
}
