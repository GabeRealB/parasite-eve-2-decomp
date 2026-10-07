#include "main/random.h"

/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Alternates hidden and visible waits until sensing selects the alert animation.
///
/// Player-body contacts raise the scene alert, reveal a fully hidden enemy and
/// request a cry and battle engagement. Alert animation repeats its cry every
/// 40 animation ticks. Nonzero placement variants select the alternate sound
/// bank; placement index identifies the sound instance. Requires a composed
/// root for spatial audio and initialized, LAST-terminated sensing tables.
static void _skullStalkerIdleTick(Task* task)
{
    enum {
        SKULL_STALKER_SOUND_APPEAR         = 0x402E0006,
        SKULL_STALKER_SOUND_APPEAR_VARIANT = 0x40480007,
        SKULL_STALKER_SOUND_ALERT          = 0x402E0007,
        SKULL_STALKER_SOUND_ALERT_VARIANT  = 0x40480008,
        SKULL_STALKER_CRY_INTERVAL_TICKS   = 40,
        SKULL_STALKER_IDLE_SCRATCH_BYTES   = 8
    };
    SkullStalkerWork* work;
    GfxCoord*         rootCoord;
    s32               soundId;
    s16               animId;
    s32               soundScript;
    Enemy*            enemy;

    /// Queues one placement-tagged spatial sound.
    ///
    /// Captures task, rootCoord, enemy, soundScript and soundId; the last three
    /// locals are overwritten. scriptId is evaluated once. Use only within a
    /// braced compound statement, with a live task and composed root coordinate.
#define SKULL_STALKER_PLAY_IDLE_SOUND(scriptId)                                      \
    enemy       = task->spawnArg2.pointer;                                           \
    soundScript = (scriptId);                                                        \
    soundId     = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundScript; \
    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord))

    work = task->work;
    // This tick reserves an extra scratch frame whose contents are never accessed.
    SCRATCH_STACK_RESERVE_BYTES(SKULL_STALKER_IDLE_SCRATCH_BYTES);
    rootCoord = task->extra.tmd->coords;
    if (worldCollisionCountContactsByKind(work->senseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0 || worldCollisionCountContactsByKind(work->frontSenseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        work->alertRequested                       = 1;
        work->animId                               = SKULL_STALKER_ANIM_ALERT;
        if (work->hiding != 0 && work->fadeFrames == SKULL_STALKER_FADE_FRAMES) {
            if (work->variant != 0) {
                SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_APPEAR_VARIANT);
            } else {
                SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_APPEAR);
            }
            work->animFrames     = 0;
            work->hiding         = 0;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->fadeWaitFrames = ((gRandomLcgState >> 16) & SKULL_STALKER_VISIBLE_WAIT_JITTER_MASK) + SKULL_STALKER_VISIBLE_WAIT_BASE;
        }
    }
    if (work->alertRequested != 0) {
        if (work->variant != 0) {
            SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_ALERT_VARIANT);
        } else {
            SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_ALERT);
        }
        work->senseBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->frontSenseBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        sceneEngageBattle(1);
    }
    // Only the sphere table is cleared. A front-capsule contact stays occupied
    // after pair testing is disabled and can request the alert again next tick.
    worldCollisionClearContacts(work->senseContacts);
    animId = work->animId;
    if (animId == SKULL_STALKER_ANIM_IDLE) {
        work->field_29A = 1;
        work->field_292 = 0;
        if (work->animFrames > work->fadeWaitFrames) {
            if (work->hiding != 0 && work->fadeFrames == SKULL_STALKER_FADE_FRAMES) {
                if (work->variant != 0) {
                    SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_APPEAR_VARIANT);
                } else {
                    SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_APPEAR);
                }
                work->animFrames     = 0;
                work->hiding         = 0;
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->fadeWaitFrames = ((gRandomLcgState >> 16) & SKULL_STALKER_VISIBLE_WAIT_JITTER_MASK) + SKULL_STALKER_VISIBLE_WAIT_BASE;
            } else if (work->fadeFrames == 0 && work->hiding == 0) {
                work->animFrames     = 0;
                work->hiding         = 1;
                gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->fadeWaitFrames = ((gRandomLcgState >> 16) & SKULL_STALKER_HIDDEN_WAIT_JITTER_MASK) + SKULL_STALKER_HIDDEN_WAIT_BASE;
            }
        }
    } else if (animId == SKULL_STALKER_ANIM_ALERT) {
        work->alertRequested = 0;
        if (work->animFrames >= SKULL_STALKER_CRY_INTERVAL_TICKS) {
            if (work->variant != 0) {
                SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_ALERT_VARIANT);
            } else {
                SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_ALERT);
            }
            work->animFrames = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(SKULL_STALKER_IDLE_SCRATCH_BYTES);
#undef SKULL_STALKER_PLAY_IDLE_SOUND
}
