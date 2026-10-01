/* Part of the Mad Chaser library; see mad_chaser.h. */

/// When the recoil ends, returns to the walk (upright) or claims the alert hold
/// and goes to the alert state.
void madChaserRecoilHeavyEnd(Task* arg0)
{
    MadChaserWork* work;
    s32            cond;

    work = (MadChaserWork*)arg0->work;
    if ((work->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->field_44F == 1) {
            work            = (MadChaserWork*)arg0->work;
            work->field_420 = 3;
            work->field_422 = 0;
        } else {
            madChaserSetAlertHold(arg0, 1);
            work            = (MadChaserWork*)arg0->work;
            work->field_420 = 5;
            work->field_422 = 0;
        }
    }
}
