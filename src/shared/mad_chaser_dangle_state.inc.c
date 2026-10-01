/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs the sub-state handler for `field_422` from a four-entry table.
void madChaserDangleState(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable4   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = gMadChaserDangleSteps;
    sp.funcs[(s16)work->field_422](arg0);
}
