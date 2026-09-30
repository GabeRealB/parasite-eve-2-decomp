/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Once the hit flags are set, moves the state machine to state 1.
void hopperLurkIdleEnd(Task* arg0)
{
    Actor341700Work* work;
    s32              cond;

    work = (Actor341700Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (Actor341700Work*)arg0->work;
        work->field_420 = 1;
        work->field_422 = 0;
    }
}
