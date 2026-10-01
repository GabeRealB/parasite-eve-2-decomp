/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Returns 1 when the hit flags are set - bit 0 of the flag halfword or bits
/// 0x102 of the word - and 0 otherwise.
s16 madChaserAnimEnded(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if ((work->flags_EC.half & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->flags_EC.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}
