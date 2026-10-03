/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests animation 0xC and advances the sub-state.
void madChaserStatusHoldStart(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    work->animBlendFrames = 8;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = 0xC;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState        = work->subState + 1;
}
