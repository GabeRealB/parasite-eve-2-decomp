/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Requests animation 9, clears the frame counter, advances the sub-state
/// and, while the enemy has HP left, plays sound 2.
void madChaserAlertCry(Task* arg0)
{
    MadChaserWork* work;
    Enemy*         enemy;
    s32            soundId;
    s32            pan;

    work                  = (MadChaserWork*)arg0->work;
    enemy                 = (Enemy*)arg0->spawnArg2.pointer;
    work->animBlendFrames = 4;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = 9;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->stateFrames     = 0;
    work->subState++;
    if (enemy->hp > 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0002;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}
