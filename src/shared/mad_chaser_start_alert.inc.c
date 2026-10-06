/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests animation 0xF, advances the sub-state and arms `gSceneCombatState`.
void madChaserStartAlert(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    work->animBlendFrames = 8;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = 0xF;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState        = work->subState + 1;
    sceneEngageBattle(1);
}
