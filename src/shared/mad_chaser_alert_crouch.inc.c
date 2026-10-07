/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once slot 1 reports a boundary, jump or hold, marks the enemy busy (`busy`), requests
/// animation 4 and advances the sub-state.
void madChaserAlertCrouch(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;

    work = (MadChaserWork*)arg0->work;
    if ((_madChaserAnimHasBoundaryStatus(arg0) << 0x10) != 0) {
        work->busy             = 1;
        work->stateFrames      = 0;
        work2                  = (MadChaserWork*)arg0->work;
        work2->animBlendFrames = 4;
        work2->animRate        = ANIMATION_RATE_ONE;
        work2->animId          = 4;
        work2->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState         = work->subState + 1;
    }
}
