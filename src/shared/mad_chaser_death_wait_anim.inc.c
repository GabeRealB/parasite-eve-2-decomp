/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Ticks the animation and, once the hit flags are set, advances the state.
void madChaserDeathWaitAnim(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s32            cond;

    work = (MadChaserWork*)arg0->work;
    _madChaserTickAnim(arg0);
    work2 = (MadChaserWork*)arg0->work;
    if ((work2->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work2->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->state = work->state + 1;
    }
}
