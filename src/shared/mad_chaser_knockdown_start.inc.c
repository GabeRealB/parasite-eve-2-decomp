/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Knockdown: reads the stance of the playing animation and requests fall
/// animation 6 (upright) or 5, then advances.
void madChaserKnockdownStart(Task* arg0)
{
    MadChaserWork* work;

    work               = (MadChaserWork*)arg0->work;
    work->stateScratch = gMadChaserAnimStance[work->animId - 1];
    if (work->stateScratch == 1) {
        MadChaserWork* w = (MadChaserWork*)arg0->work;

        w->animBlendFrames = 6;
        w->animRate        = ANIMATION_RATE_ONE;
        w->animId          = 6;
        w->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    } else {
        MadChaserWork* w = (MadChaserWork*)arg0->work;

        w->animBlendFrames = 6;
        w->animRate        = ANIMATION_RATE_ONE;
        w->animId          = 5;
        w->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    work->subState++;
}
