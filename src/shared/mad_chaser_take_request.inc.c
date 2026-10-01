/* Part of the Mad Chaser library; see mad_chaser.h. */

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `field_41E` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
s32 madChaserTakeHitRequest(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->field_420     = 6;
                w->field_422     = 0;
                break;
            }
            case 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->field_420     = 7;
                w->field_422     = 0;
                break;
            }
            case 2: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->field_420     = 8;
                w->field_422     = 0;
                break;
            }
            case 3: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->field_420     = 7;
                w->field_422     = 0;
                break;
            }
            case 4: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->field_420     = 9;
                w->field_422     = 0;
                break;
            }
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}
