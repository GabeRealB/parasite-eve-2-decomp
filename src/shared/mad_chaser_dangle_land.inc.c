/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays sounds 4 and 3 on the first two frames; once the hit flags are set,
/// moves the task to state 3 with the state machine at state 3.
void madChaserDangleLand(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* next;
    MadChaserWork* next2;
    u32            soundId;
    s32            pan;

    work = (MadChaserWork*)arg0->work;
    if ((s16)++work->stateFrames == 1) {
        soundId   = (u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0004;
        pan       = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan     >>= 24;
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->stateFrames == 2) {
        soundId   = (u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0003;
        pan       = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan     >>= 24;
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (madChaserAnimEnded(arg0)) {
        next            = (MadChaserWork*)arg0->work;
        arg0->state     = 3;
        next->state     = 0;
        next->subState  = 0;
        next2           = (MadChaserWork*)arg0->work;
        next2->state    = 3;
        next2->subState = 0;
    }
}
