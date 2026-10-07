/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once slot 1 reports a boundary, jump or hold, requests animation 7 (when `stateScratch` is 1)
/// or 1, and advances the sub-state.
void madChaserKnockdownRise(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* slow;
    MadChaserWork* fast;

    work = (MadChaserWork*)arg0->work;
    if (_madChaserAnimHasBoundaryStatus(arg0) != 0) {
        if (work->stateScratch == 1) {
            fast                  = (MadChaserWork*)arg0->work;
            fast->animBlendFrames = 0x32;
            fast->animRate        = ANIMATION_RATE_ONE;
            fast->animId          = 7;
            fast->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        } else {
            slow                  = (MadChaserWork*)arg0->work;
            slow->animBlendFrames = 0x1E;
            slow->animRate        = ANIMATION_RATE_ONE;
            slow->animId          = 1;
            slow->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        }
        work->subState = work->subState + 1;
    }
}
