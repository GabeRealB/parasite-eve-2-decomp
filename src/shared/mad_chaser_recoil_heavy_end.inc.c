/* Part of the Mad Chaser library; see mad_chaser.h. */

/// When the recoil ends, returns to the walk (upright) or claims the alert hold
/// and goes to the alert state.
void madChaserRecoilHeavyEnd(Task* arg0)
{
    MadChaserWork* work;
    s32            cond;

    work = (MadChaserWork*)arg0->work;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->stateScratch == 1) {
            work           = (MadChaserWork*)arg0->work;
            work->state    = 3;
            work->subState = 0;
        } else {
            _madChaserSetAlertHold(arg0, 1);
            work           = (MadChaserWork*)arg0->work;
            work->state    = 5;
            work->subState = 0;
        }
    }
}
