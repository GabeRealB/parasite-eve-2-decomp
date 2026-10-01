/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Unless `madChaserTakeHitRequest` consumes a pending request, runs the
/// sub-state handler for `field_422` from a three-entry table.
void madChaserWalkState(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = gMadChaserWalkSteps;
    if ((madChaserTakeHitRequest(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}
