/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hit flags are set, moves the state machine to state 3 (when
/// `field_44F` is 1) or 5.
void madChaserKnockdownEnd(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;

    if (madChaserAnimEnded(arg0)) {
        if (work->field_44F == 1) {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor341700Work* w = (Actor341700Work*)arg0->work;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}
