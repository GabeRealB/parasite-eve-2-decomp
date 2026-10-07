/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Lurk state 3: if `_madChaserJoinAlert` moves the Mad Chaser into the alert it
/// clears `busy`; otherwise it runs the `subState` sub-state from a
/// three-entry table.
void madChaserLurkAlertState(Task* arg0)
{
    MadChaserWork* work;
    TaskFuncTable3 sp;

    work = (MadChaserWork*)arg0->work;
    sp   = gMadChaserLurkAlertSteps;
    if ((_madChaserJoinAlert(arg0) << 0x10) != 0) {
        work->busy = 0;
        return;
    }
    sp.funcs[(s16)work->subState](arg0);
}
