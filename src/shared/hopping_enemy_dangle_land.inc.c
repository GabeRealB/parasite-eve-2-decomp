/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Plays sounds 4 and 3 on the first two frames; once the hit flags are set,
/// moves the task to state 3 with the state machine at state 3.
void hopperDangleLand(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* next;
    Actor341700Work* next2;
    u32              soundId;
    s32              pan;

    work = (Actor341700Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId   = (u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0004;
        pan       = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((s16)work->field_412 == 2) {
        soundId   = (u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x402C0003;
        pan       = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan     >>= 24;
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (hopperAnimEnded(arg0)) {
        next             = (Actor341700Work*)arg0->work;
        arg0->state      = 3;
        next->field_420  = 0;
        next->field_422  = 0;
        next2            = (Actor341700Work*)arg0->work;
        next2->field_420 = 3;
        next2->field_422 = 0;
    }
}
