/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Unless `madChaserTakeHitRequest` consumes a pending request, runs the
/// sub-state handler for `subState` from a three-entry table.
void madChaserWalkState(Task* arg0)
{
    MadChaserWork* work;
    TaskFuncTable3 sp;

    work = (MadChaserWork*)arg0->work;
    sp   = gMadChaserWalkSteps;
    if ((madChaserTakeHitRequest(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->subState](arg0);
    }
}
