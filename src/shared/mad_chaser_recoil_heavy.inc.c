/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Heavy recoil: as the light one, with animation 0xC blended over 2 frames for
/// the upright stance.
void madChaserRecoilHeavy(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s32            soundId;
    s32            pan;

    work               = (MadChaserWork*)arg0->work;
    work->stateScratch = gMadChaserAnimStance[work->animId - 1];
    if (work->stateScratch == 1) {
        work2                  = (MadChaserWork*)arg0->work;
        work2->animBlendFrames = 2;
        work2->animRate        = ANIMATION_RATE_ONE;
        work2->animId          = 0xC;
        work2->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    } else {
        work2                  = (MadChaserWork*)arg0->work;
        work2->animBlendFrames = 8;
        work2->animRate        = ANIMATION_RATE_ONE;
        work2->animId          = 0x11;
        work2->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0003;
    pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->subState++;
}
