/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hit flags are set, moves the state machine to state 1.
void madChaserLurkIdleEnd(Task* arg0)
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
        work            = (MadChaserWork*)arg0->work;
        work->field_420 = 1;
        work->field_422 = 0;
    }
}
