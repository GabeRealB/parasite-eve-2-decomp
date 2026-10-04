/* Part of the Mad Chaser library; see mad_chaser.h. */

/// In the flat stance requests animation 9, plays sound 2 and skips to the limp
/// drag; otherwise requests animation 7, seeds the speed ramp and advances to
/// the struggle.
void madChaserPullReact(Task* arg0)
{
    MadChaserWork* work;
    s32            soundId;
    s32            pan;

    work = (MadChaserWork*)arg0->work;
    if (gMadChaserAnimStance[work->animId - 1] == 0) {
        work->animBlendFrames = 4;
        work->animRate        = ANIMATION_RATE_ONE;
        work->animId          = 9;
        work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        soundId               = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0002;
        pan                   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->subState = 4;
        return;
    }
    work->animBlendFrames = 8;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = 7;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->stateScratch    = (u8)work->animRate * 4;
    work->subState++;
}
