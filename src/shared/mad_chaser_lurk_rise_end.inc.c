/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hit flags are set, returns the state machine to state 0.
void madChaserLurkRiseEnd(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s32            cond;

    work = (MadChaserWork*)arg0->work;
    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2           = (MadChaserWork*)arg0->work;
        work2->state    = 0;
        work2->subState = 0;
    }
}
