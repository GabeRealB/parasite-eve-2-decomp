/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Finishes an ordinary leap landing, turns away and returns to idle lurk.
///
/// Requires live work and enemy/model storage with stateFrames reset on landing.
/// Plays the positional landing cue on frame 1. A slot-1 boundary, jump or held
/// pose clears busy, adds a half turn, stores a 90..217 combat-tick leap cooldown
/// from one random draw and resets clip 13 at normal rate. Enters the lurk task
/// at behavior/sub-state zero; the caller ticks the requested animation.
static void _madChaserLeapTurnAway(Task* task)
{
    enum {
        MAD_CHASER_LEAP_COOLDOWN_MIN_FRAMES  = 90,
        MAD_CHASER_LEAP_COOLDOWN_RANDOM_MASK = 127,
        MAD_CHASER_LEAP_LANDING_SOUND        = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 4),
        MAD_CHASER_LEAP_TURN_AWAY_CLIP       = 13,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            soundId;
    s32            audioPan;
    u32            randomState;

    work = task->work;
    if ((s16)++work->stateFrames == 1) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_LEAP_LANDING_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (_madChaserAnimHasBoundaryStatus(task) != 0) {
        work->busy               = 0;
        randomState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->leapCooldown       = ((randomState >> 16) & MAD_CHASER_LEAP_COOLDOWN_RANDOM_MASK) + MAD_CHASER_LEAP_COOLDOWN_MIN_FRAMES;
        work->rotation.vy       += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        requestWork              = task->work;
        requestWork->animRate    = ANIMATION_RATE_ONE;
        requestWork->animId      = MAD_CHASER_LEAP_TURN_AWAY_CLIP;
        requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
        gRandomLcgState          = randomState;
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_LURK);
    }
}
