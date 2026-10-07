/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Releases the gSceneCombatState reference and requests the settle animation that
/// follows the playing one (5 or 6 after animation 8), then ticks it and
/// advances.
void madChaserDeathSettle(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    MadChaserWork* work3;
    MadChaserWork* work4;
    s16            anim;
    s16            next;

    work = (MadChaserWork*)arg0->work;
    sceneReleaseBattleRefWithRewards(arg0, 0);
    anim = work->animId;
    if (anim == 8) {
        if (work->hasLeaped == 0) {
            work2                  = (MadChaserWork*)arg0->work;
            work2->animBlendFrames = 4;
            work2->animRate        = ANIMATION_RATE_ONE;
            work2->animId          = 5;
            work2->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        } else {
            work3                  = (MadChaserWork*)arg0->work;
            work3->animBlendFrames = 4;
            work3->animRate        = ANIMATION_RATE_ONE;
            work3->animId          = 6;
            work3->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        }
    } else {
        next                   = gMadChaserSettleAnims[anim - 1];
        work4                  = (MadChaserWork*)arg0->work;
        work4->animBlendFrames = 4;
        work4->animRate        = ANIMATION_RATE_ONE;
        work4->animId          = next;
        work4->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    madChaserTickAnim(arg0);
    work->state++;
}
