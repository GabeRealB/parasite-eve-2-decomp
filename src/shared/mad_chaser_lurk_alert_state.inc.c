/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Lurk state 3: if `madChaserJoinAlert` moves the Mad Chaser into the alert it
/// clears `field_438`; otherwise it runs the `field_422` sub-state from a
/// three-entry table.
void madChaserLurkAlertState(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->work;
    sp   = gMadChaserLurkAlertSteps;
    if ((madChaserJoinAlert(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}
