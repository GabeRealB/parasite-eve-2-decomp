/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Unless `_madChaserJoinAlert` takes over, requests animation 0xF
/// and advances the sub-state.
void madChaserLurkShiftStart(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;

    work = (MadChaserWork*)arg0->work;
    if (_madChaserJoinAlert(arg0) == 0) {
        work2                  = (MadChaserWork*)arg0->work;
        work2->animBlendFrames = 8;
        work2->animRate        = ANIMATION_RATE_ONE;
        work2->animId          = 0xF;
        work2->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState         = work->subState + 1;
    }
}
