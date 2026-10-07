/* Part of the Mad Chaser library; see mad_chaser.h. */

/// madChaserDeathSettle without releasing the gSceneCombatState reference: requests the
/// follow-up settle animation, ticks it and advances.
void madChaserDeathSettleQuiet(Task* arg0)
{
    MadChaserWork* work;
    s16            anim;
    s16            next;

    work = (MadChaserWork*)arg0->work;
    anim = work->animId;
    if (anim == 8) {
        if (work->hasLeaped == 0) {
            work->animBlendFrames = 4;
            work->animRate        = ANIMATION_RATE_ONE;
            work->animId          = 5;
            work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        } else {
            work->animBlendFrames = 4;
            work->animRate        = ANIMATION_RATE_ONE;
            work->animId          = 6;
            work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        }
    } else {
        next                  = gMadChaserSettleAnims[anim - 1];
        work->animBlendFrames = 4;
        work->animRate        = ANIMATION_RATE_ONE;
        work->animId          = next;
        work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    _madChaserTickAnim(arg0);
    work->state++;
}
