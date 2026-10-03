/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Sets `anchored`, requests animation 7 and advances the sub-state.
void madChaserDangleStart(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;

    work               = (MadChaserWork*)arg0->work;
    work->anchored     = 1;
    work2              = (MadChaserWork*)arg0->work;
    work2->animRate    = ANIMATION_RATE_ONE;
    work2->animId      = 7;
    work2->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    work->subState     = work->subState + 1;
}
