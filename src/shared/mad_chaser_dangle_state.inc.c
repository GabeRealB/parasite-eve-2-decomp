/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs the sub-state handler for `field_422` from a four-entry table.
void madChaserDangleState(Task* arg0)
{
    MadChaserWork* work;
    TaskFuncTable4 sp;

    work = (MadChaserWork*)arg0->work;
    sp   = gMadChaserDangleSteps;
    sp.funcs[(s16)work->field_422](arg0);
}
