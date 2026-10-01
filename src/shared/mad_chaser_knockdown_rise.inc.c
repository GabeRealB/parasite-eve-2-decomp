/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hit flags are set, requests animation 7 (when `field_44F` is 1)
/// or 1, and advances the sub-state.
void madChaserKnockdownRise(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* slow;
    MadChaserWork* fast;

    work = (MadChaserWork*)arg0->work;
    if (madChaserAnimEnded(arg0) != 0) {
        if (work->field_44F == 1) {
            fast            = (MadChaserWork*)arg0->work;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (MadChaserWork*)arg0->work;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}
