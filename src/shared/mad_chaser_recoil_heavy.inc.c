/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Heavy recoil: as the light one, with animation 0xC blended over 2 frames for
/// the upright stance.
void madChaserRecoilHeavy(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s32            soundId;
    s32            pan;

    work            = (MadChaserWork*)arg0->work;
    work->field_44F = gMadChaserAnimStance[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (MadChaserWork*)arg0->work;
        work2->field_426 = 2;
        work2->field_41C = 0x10;
        work2->field_418 = 0xC;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(SOUND_MAD_CHASER_ALERT_CRY, 1);
    } else {
        work2            = (MadChaserWork*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0x11;
        work2->field_414 = 1;
    }
    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0003;
    pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->field_422++;
}
