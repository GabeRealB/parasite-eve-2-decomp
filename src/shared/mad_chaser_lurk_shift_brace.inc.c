/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Unless `_madChaserJoinAlert` takes over, waits for the hit flags,
/// then marks the enemy busy, requests animation 4 and advances the
/// sub-state.
void madChaserLurkShiftBrace(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    MadChaserWork* work3;
    s32            cond;

    work = (MadChaserWork*)arg0->work;
    if ((_madChaserJoinAlert(arg0) << 0x10) == 0) {
        work2 = (MadChaserWork*)arg0->work;
        if ((work2->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
            (work2->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->stateFrames      = 0;
            work->busy             = 1;
            work3                  = (MadChaserWork*)arg0->work;
            work3->animBlendFrames = 4;
            work3->animRate        = ANIMATION_RATE_ONE;
            work3->animId          = 4;
            work3->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            work->subState         = work->subState + 1;
        }
    }
}
