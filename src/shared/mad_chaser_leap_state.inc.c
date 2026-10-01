/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs the sub-state handler for `field_422` from a five-entry table.
void madChaserLeapState(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable5   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = gMadChaserLeapSteps;
    sp.funcs[(s16)work->field_422](arg0);
}
