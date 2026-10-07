/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Finishes the rebound landing and returns to combat walking.
///
/// Requires live work and enemy/model storage with stateFrames reset on landing.
/// Clears busy each call and plays the positional landing cue on frame 1. A
/// slot-1 boundary, jump or held pose stores a 90..217 combat-tick leap cooldown
/// from one random draw, selects walk behavior and resets its sub-state. The
/// caller ticks animation; the u16 frame counter retains its signed comparison.
static void _madChaserLeapLand(Task* task)
{
    enum {
        MAD_CHASER_LEAP_COOLDOWN_MIN_FRAMES  = 90,
        MAD_CHASER_LEAP_COOLDOWN_RANDOM_MASK = 127,
        MAD_CHASER_LEAP_LANDING_SOUND        = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 4),
    };
    MadChaserWork* work;
    s32            soundId;
    s32            audioPan;
    u32            randomState;

    work       = task->work;
    work->busy = 0;
    if ((s16)++work->stateFrames == 1) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_LEAP_LANDING_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (_madChaserAnimHasBoundaryStatus(task) != 0) {
        randomState        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState    = randomState;
        work->leapCooldown = ((randomState >> 16) & MAD_CHASER_LEAP_COOLDOWN_RANDOM_MASK) + MAD_CHASER_LEAP_COOLDOWN_MIN_FRAMES;
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
    }
}
