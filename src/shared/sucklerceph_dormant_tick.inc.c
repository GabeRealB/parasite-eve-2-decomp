#include "main/random.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Schedules the dormant Sucklerceph's next idle sound and advances its countdown.
///
/// Requires the task's live work, enemy and root coordinate and a loaded sound
/// bank. Each request draws the next delay from 80..179 frames and uses the
/// enemy's placement index as its sound instance tag; request failure is ignored.
static __inline__ void _sucklercephTickIdleSound(const Task* task, const GfxCoord* rootCoord, SucklercephWork* work)
{
    enum {
        SUCKLERCEPH_IDLE_SOUND_MIN_FRAMES    = 80,
        SUCKLERCEPH_IDLE_SOUND_FRAME_CHOICES = 100,
        SUCKLERCEPH_IDLE_SOUND               = 0x402E0001,
        SUCKLERCEPH_VARIANT_IDLE_SOUND       = 0x40460009,
        SUCKLERCEPH_SOUND_INSTANCE_SHIFT     = 8
    };
    s32    soundId;
    u32    randomState;
    Enemy* enemy;
    /// Requests the chosen idle script at the enemy's current audio position.
    ///
    /// Block statement macro capturing task, rootCoord, enemy and soundId.
    /// Requires stable live pointers and SUCKLERCEPH_SOUND_INSTANCE_SHIFT.
    /// Evaluates baseSoundId once; narrows pan/depth to s8 and ignores the
    /// request result. Undefined after this function.
#define SUCKLERCEPH_REQUEST_IDLE_SOUND(baseSoundId)                                                                                  \
    {                                                                                                                                \
        enemy   = task->spawnArg2.pointer;                                                                                           \
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_SOUND_INSTANCE_SHIFT) | (baseSoundId);                \
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord)); \
    }
    work->idleSoundFrames--;
    if (work->idleSoundFrames <= 0) {
        randomState           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState       = randomState;
        work->idleSoundFrames = (randomState >> 16) % SUCKLERCEPH_IDLE_SOUND_FRAME_CHOICES + SUCKLERCEPH_IDLE_SOUND_MIN_FRAMES;
        if (work->variant != 0) {
            SUCKLERCEPH_REQUEST_IDLE_SOUND(SUCKLERCEPH_VARIANT_IDLE_SOUND);
        } else {
            SUCKLERCEPH_REQUEST_IDLE_SOUND(SUCKLERCEPH_IDLE_SOUND);
        }
    }
}

#undef SUCKLERCEPH_REQUEST_IDLE_SOUND

/// Rocks an idle Sucklerceph until player contact or damage requests waking.
///
/// Requires an initialized root, work and owning enemy. Waking selects crawl,
/// disables the sense sphere's pair tests and engages the scene battle; sense
/// contacts are consumed on every call. Under the idle animation, sound repeats
/// after 80..179 frames, the root moves forward during frames 1..41 and backward
/// during 51..91 at 20 units per frame, and the cycle resets at frame 99.
/// Holds eight untouched scratch bytes across the whole update; their role
/// is unproven. Collision correction and animation playback follow in the caller.
static void _sucklercephDormantTick(Task* task)
{
    enum {
        SUCKLERCEPH_DORMANT_SCRATCH_BYTES = 8,
        SUCKLERCEPH_ROCK_SPEED            = 20,
        SUCKLERCEPH_ROCK_WINDOW_FRAMES    = 41,
        SUCKLERCEPH_ROCK_BACKWARD_START   = 51,
        SUCKLERCEPH_ROCK_CYCLE_FRAMES     = 99
    };

    SucklercephWork* work;
    GfxCoord*        rootCoord;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    SCRATCH_STACK_RESERVE_BYTES(SUCKLERCEPH_DORMANT_SCRATCH_BYTES);
    if (worldCollisionCountContactsByKind(&work->senseContact, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        work->wakeRequested = 1;
    }
    if (work->wakeRequested != 0) {
        work->state           = SUCKLERCEPH_STATE_AWAKE;
        work->awakeStage      = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
        work->senseBody.flags = (u16)(work->senseBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        sceneEngageBattle(1);
    }
    worldCollisionClearContacts(&work->senseContact);
    if (work->animId == SUCKLERCEPH_ANIM_IDLE) {
        _sucklercephTickIdleSound(task, rootCoord, work);
        work->field_2C6    = 1;
        work->forwardSpeed = 0;
        if ((u32)(work->animFrames - 1) < SUCKLERCEPH_ROCK_WINDOW_FRAMES) {
            work->forwardSpeed = SUCKLERCEPH_ROCK_SPEED;
        }
        if ((u32)(work->animFrames - SUCKLERCEPH_ROCK_BACKWARD_START) < SUCKLERCEPH_ROCK_WINDOW_FRAMES) {
            work->forwardSpeed = -SUCKLERCEPH_ROCK_SPEED;
        }
        if ((s16)work->animFrames >= SUCKLERCEPH_ROCK_CYCLE_FRAMES) {
            work->animFrames = 0;
        }
        _sucklercephStep(task);
    }
    SCRATCH_STACK_RELEASE_BYTES(SUCKLERCEPH_DORMANT_SCRATCH_BYTES);
}
