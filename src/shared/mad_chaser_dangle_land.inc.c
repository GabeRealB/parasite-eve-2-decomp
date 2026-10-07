/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays a landing cue on this enemy's positional character-bank channel.
///
/// Requires live enemy/model storage and a character-bank cue without channel
/// bits set. Pan and depth are signed bytes; the place index supplies channel
/// bits 8..11. Borrows the task and queues the sound without advancing state.
static __inline__ void _madChaserDangleLandPlayCue(Task* task, u32 cue)
{
    u32 soundId;
    s32 audioPan;

    soundId    = ((Enemy*)task->spawnArg2.pointer)->placeKey;
    soundId  >>= ENEMY_PLACE_INDEX_SHIFT;
    soundId  <<= 8;
    soundId   |= cue;
    audioPan   = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
    audioPan >>= 24;
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Completes the dangle landing animation and enters the combat walk.
///
/// Requires live enemy/model/work storage, initialized animation slots and a
/// stateFrames counter reset by the fall. Increments the wrapping u16 counter
/// and plays positional character-bank cues 4 and 3 on signed frames 1 and 2.
/// A slot-1 boundary, control jump or held pose enters combat at its walk
/// behavior and sub-state zero; the caller applies animation and collision.
static void _madChaserDangleLand(Task* task)
{
    MadChaserWork* work;

    work = task->work;
    if ((s16)++work->stateFrames == 1) {
        _madChaserDangleLandPlayCue(task, SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 4));
    }
    if ((s16)work->stateFrames == 2) {
        _madChaserDangleLandPlayCue(task, SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 3));
    }
    if (_madChaserAnimHasBoundaryStatus(task)) {
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
    }
}
