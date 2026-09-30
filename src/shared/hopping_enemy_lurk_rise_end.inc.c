/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Once the hit flags are set, returns the state machine to state 0.
void hopperLurkRiseEnd(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 0;
        work2->field_422 = 0;
    }
}
