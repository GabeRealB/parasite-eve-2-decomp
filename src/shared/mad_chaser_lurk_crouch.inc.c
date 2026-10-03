/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Once the hit flags are set, requests animation 0xD and advances the
/// sub-state.
void madChaserLurkCrouch(Task* arg0)
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
        work2                  = (MadChaserWork*)arg0->work;
        work2->animBlendFrames = 8;
        work2->animRate        = ANIMATION_RATE_ONE;
        work2->animId          = 0xD;
        work2->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->subState++;
    }
}
