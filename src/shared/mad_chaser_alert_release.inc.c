/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hit flags are set, releases this enemy's `gSceneCombatState` hold,
/// requests animation 0xF and advances the sub-state.
void madChaserAlertRelease(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    work = (Actor341700Work*)arg0->work;
    if ((madChaserAnimEnded(arg0) << 0x10) != 0) {
        madChaserSetAlertHold(arg0, 0);
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}
