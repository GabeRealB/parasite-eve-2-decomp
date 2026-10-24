#include "main/random.h"
#include "main/sound_ids.h"

/* Part of the Glutton library; see glutton.h. */

/// Links the rain blob's unscaled attack sphere with pair testing disabled.
///
/// Requires an unlinked attack body and a zero-initialized body coordinate in
/// live work. Preserves the body's preselected attack key, copies the local
/// centre's XYZ, and clears the whole contact table. Radius is an unsigned
/// 16-bit world-unit value; the linked work and embedded coordinate must
/// remain live until unlink. The caller enables pairing after ascent.
static inline void _gluttonLinkRainAttackSphere(GluttonProjectileWork* work, const SVECTOR* localCenter, u16 radius)
{
    work->bodyCoord.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&work->bodyCoord.coord);
    gfxRotMatrixY(&work->bodyCoord.coord, 0, GRAPHICS_ROTATION_REPLACE);

    _worldCollisionLinkSphereBody(&work->bodyCoord, &work->attackBody, work->attackContacts, localCenter, radius, WORLD_COLLISION_LIST_ENEMY_ATTACKS,
                                  ARRAY_SIZE(work->attackContacts));
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Creates a rain blob's launch coordinate, landing point and child effect.
///
/// Requires a live coordinate body, parent host TMD body with attack entry 1,
/// and player TMD coordinate in the view frame. The low half of `spawnArg1`
/// must be 0..7: index 0 rerolls the shared landing-pattern group and index 4
/// targets the player's current X/Z. Other targets use a distance-selected X
/// lead plus the pattern's swapped X/Z components and a Z jitter of 0..127.
/// Allocates zeroed task-owned work and links a separate attack coordinate
/// with pair testing disabled until ascent ends. A successful visible-effect
/// spawn becomes this task's child. Shutdown or allocation failure destroys
/// the enemy; success advances to ascent. Positions use world units.
/// Distance selection squares signed-halfword XYZ offsets in 32-bit arithmetic.
static void _gluttonRainSpawn(Enemy* enemy, Task* task)
{
    enum { GLUTTON_RAIN_FIRST_BLOB_INDEX    = 0,
           GLUTTON_RAIN_PLAYER_TARGET_INDEX = 4,
           GLUTTON_RAIN_NEAR_DISTANCE       = 8000,
           GLUTTON_RAIN_FAR_DISTANCE        = 11000,
           GLUTTON_RAIN_NEAR_X_LEAD         = 7000,
           GLUTTON_RAIN_MIDDLE_X_LEAD       = 10000,
           GLUTTON_RAIN_FAR_X_LEAD          = 13000,
           GLUTTON_RAIN_PATTERN_Z_OFFSET    = 6300,
           GLUTTON_RAIN_LANDING_JITTER_MASK = 127,
           GLUTTON_RAIN_SPEED_JITTER_MASK   = 8,
           GLUTTON_RAIN_INITIAL_RADIUS      = 256,
           GLUTTON_RAIN_ATTACK_INDEX        = 1,
           GLUTTON_RAIN_LAUNCH_SOUND        = SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0B) };
    GluttonProjectileWork* work;
    Enemy*                 hostEnemy;
    Task*                  hostTask;
    Task*                  playerTask;
    SVECTOR                offset;
    s32                    playerDistance;
    s32                    patternRoll;
    s32                    soundId;
    s32                    audioPan;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    hostEnemy  = task->parent->spawnArg2.pointer;
    hostTask   = task->parent;

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
        enemyDestroy(enemy, task);
        return;
    }
    work       = memCalloc(sizeof(GluttonProjectileWork), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }

    task->extra.coordBody->coord->parent = &gGfxViewCoord;
    work->fallStep                       = 0;

    // This initial sample is overwritten by the host sample below, but belongs
    // to the retained spawn sequence.
    _gluttonGetPlayerOffset(task->extra.coordBody->coord, &offset);

    // Only the first of the eight launches chooses a pattern for the whole set.
    if ((u16)task->spawnArg1.value == GLUTTON_RAIN_FIRST_BLOB_INDEX) {
        RANDOM_LCG_ADVANCE();
        patternRoll = RANDOM_LCG_DRAW() & 3;
        switch (patternRoll) {
            case 0:
            case 1:
                _gGluttonRainGroup = 1;
                break;
            case 2:
                _gGluttonRainGroup = patternRoll;
                break;
            case 3:
                _gGluttonRainGroup = 0;
                break;
            default:
                _gGluttonRainGroup = 0;
                break;
        }
    }

    _gluttonGetPlayerOffset(hostTask->extra.tmd->coords, &offset);
    playerDistance  = offset.vx * offset.vx;
    playerDistance += offset.vy * offset.vy;
    playerDistance += offset.vz * offset.vz;
    playerDistance  = SquareRoot0(playerDistance);

    // Pattern Z offsets run along arena X; pattern X offsets run along arena Z.
    if (playerDistance < GLUTTON_RAIN_NEAR_DISTANCE) {
        work->aim.landing.vx = hostTask->extra.tmd->coords->coord.t[0] + GLUTTON_RAIN_NEAR_X_LEAD;
    } else if (playerDistance < GLUTTON_RAIN_FAR_DISTANCE) {
        work->aim.landing.vx = hostTask->extra.tmd->coords->coord.t[0] + GLUTTON_RAIN_MIDDLE_X_LEAD;
    } else {
        work->aim.landing.vx = hostTask->extra.tmd->coords->coord.t[0] + GLUTTON_RAIN_FAR_X_LEAD;
    }

    work->aim.landing.vx +=
        _gGluttonRainPoints[_gGluttonRainPointIndex[_gGluttonRainGroup]
                                                   [(u16)task->spawnArg1.value]]
            .vz;
    work->aim.landing.vy = 0;
    RANDOM_LCG_ADVANCE();
    work->aim.landing.vz = _gGluttonRainPoints[_gGluttonRainPointIndex[_gGluttonRainGroup]
                                                                      [(u16)task->spawnArg1.value]]
                               .vx -
                           GLUTTON_RAIN_PATTERN_Z_OFFSET;
    work->aim.landing.vz = (RANDOM_LCG_DRAW() & GLUTTON_RAIN_LANDING_JITTER_MASK) + work->aim.landing.vz;

    if ((u16)task->spawnArg1.value == GLUTTON_RAIN_PLAYER_TARGET_INDEX) {
        work->aim.landing.vx = playerTask->extra.tmd->coords->coord.t[0];
        work->aim.landing.vy = 0;
        work->aim.landing.vz = playerTask->extra.tmd->coords->coord.t[2];
    }

    work->stateTicks = 0;
    RANDOM_LCG_ADVANCE();
    work->speedJitter = RANDOM_LCG_DRAW() & GLUTTON_RAIN_SPEED_JITTER_MASK;

    offset.vx = _gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vx;
    offset.vy = _gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vy;
    offset.vz = _gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vz;

    task->extra.coordBody->coord->coord.t[0] = offset.vx + hostTask->extra.tmd->coords->coord.t[0];
    task->extra.coordBody->coord->coord.t[1] = offset.vy;
    task->extra.coordBody->coord->coord.t[2] = offset.vz + hostTask->extra.tmd->coords->coord.t[2];

    work->attackBody.key = damagePackEnemyAttackKey(hostEnemy, GLUTTON_RAIN_ATTACK_INDEX);

    offset.vx = 0;
    offset.vy = 0;
    offset.vz = 0;

    // Keep the attack sphere independent of the blob's squash and stretch.
    _gluttonLinkRainAttackSphere(work, &offset, GLUTTON_RAIN_INITIAL_RADIUS);

    soundId  = ((hostEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | GLUTTON_RAIN_LAUNCH_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.coordBody->coord);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.coordBody->coord));

    work->rainEffect = effectSpawn(EFFECT_GLUTTON_RAIN_BLOB, task->extra.coordBody->coord, EFFECT_GLUTTON_RAIN_BLOB_START, NULL);
    if (work->rainEffect != NULL) {
        taskReparent(task, work->rainEffect->task);
    }
    task->state++;
}

#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Raises a rain blob out of view, then positions it over its landing point.
///
/// Moves its task coordinate upward by 500 plus `speedJitter` world units per
/// tick. Crossing y = -20000 clamps there, selects the landing x/z, clears the
/// state timer, rerolls drop jitter to 0..31 and enables the attack body before
/// advancing to descent. The body's independent coordinate follows the blob.
/// Requires a live coordinate body and initialized `GluttonProjectileWork`;
/// encounter shutdown unlinks the attack body and destroys the enemy.
static void _gluttonRainRise(Enemy* enemy, Task* task)
{
    enum { GLUTTON_RAIN_CLIMB_SPEED      = 500,
           GLUTTON_RAIN_TOP_Y            = -20000,
           GLUTTON_RAIN_DROP_JITTER_MASK = 31 };
    GluttonProjectileWork* work;
    s32                    nextY;

    work = task->work;
    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
        worldCollisionUnlinkBody(&work->attackBody);
        enemyDestroy(enemy, task);
        return;
    }

    nextY                                    = task->extra.coordBody->coord->coord.t[1] - GLUTTON_RAIN_CLIMB_SPEED;
    task->extra.coordBody->coord->coord.t[1] = nextY - work->speedJitter;
    // Start collision tests only after moving over the chosen floor point.
    if (task->extra.coordBody->coord->coord.t[1] < GLUTTON_RAIN_TOP_Y) {
        task->state++;
        task->extra.coordBody->coord->coord.t[0] = work->aim.landing.vx;
        task->extra.coordBody->coord->coord.t[2] = work->aim.landing.vz;
        RANDOM_LCG_ADVANCE();
        task->extra.coordBody->coord->coord.t[1] = GLUTTON_RAIN_TOP_Y;
        work->stateTicks                         = 0;
        work->speedJitter                        = RANDOM_LCG_DRAW() & GLUTTON_RAIN_DROP_JITTER_MASK;
        work->attackBody.flags                  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }

    task->extra.coordBody->coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->bodyCoord.coord.t[0]                 = task->extra.coordBody->coord->coord.t[0];
    work->bodyCoord.coord.t[1]                 = task->extra.coordBody->coord->coord.t[1];
    work->bodyCoord.coord.t[2]                 = task->extra.coordBody->coord->coord.t[2];
    work->bodyCoord.composeStamp               = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->bodyCoord);
}

/// Draws a rain blob's growing floor shadow and drops it into its landing burst.
///
/// Descent begins on tick 20, at 600 plus `speedJitter` world units per tick.
/// Crossing floor y = 0 clamps there, resets the timer, signals a live child
/// effect to burst, plays the owner's landing sound and advances to splatting.
/// The attack body's independent coordinate follows the blob and its contacts
/// are cleared each tick. Requires a live coordinate body, parent enemy and
/// initialized `GluttonProjectileWork`; shutdown unlinks and destroys the blob.
static void _gluttonRainFall(Enemy* enemy, Task* task)
{
    enum { GLUTTON_RAIN_SHADOW_LEAD_TICKS = 20,
           GLUTTON_RAIN_DROP_SPEED        = 600 };
    GluttonProjectileWork* work;
    GfxCoord               shadowCoord;
    Enemy*                 owner;
    s32                    soundId;
    s32                    audioPan;

    work = task->work;
    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
        worldCollisionUnlinkBody(&work->attackBody);
        enemyDestroy(enemy, task);
        return;
    }

    work->stateTicks++;
    shadowCoord.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&shadowCoord.coord);
    gfxRotMatrixY(&shadowCoord.coord, 0, GRAPHICS_ROTATION_REPLACE);

    shadowCoord.coord.t[0]   = task->extra.coordBody->coord->coord.t[0];
    shadowCoord.coord.t[1]   = 0;
    shadowCoord.coord.t[2]   = task->extra.coordBody->coord->coord.t[2];
    shadowCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&shadowCoord);

    // Warn of the landing point while the blob waits overhead.
    effectDrawGroundShadow(GRAPHICS_MATRIX_TRANSLATION(&shadowCoord.workm), (s16)(work->stateTicks * 8 + 0x80),
                           gRoomEffectState->groundShadowShade);

    if (work->stateTicks >= GLUTTON_RAIN_SHADOW_LEAD_TICKS) {
        task->extra.coordBody->coord->coord.t[1] =
            task->extra.coordBody->coord->coord.t[1] + (work->speedJitter + GLUTTON_RAIN_DROP_SPEED);
        if (task->extra.coordBody->coord->coord.t[1] > 0) {
            owner                                    = task->parent->spawnArg2.pointer;
            task->extra.coordBody->coord->coord.t[1] = 0;
            work->stateTicks                         = 0;
            if (work->rainEffect != NULL) {
                work->rainEffect->task->spawnArg1.value = EFFECT_GLUTTON_RAIN_BLOB_BURST;
            }
            task->state++;
            soundId  = ((owner->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0C);
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.coordBody->coord);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.coordBody->coord));
        }
    }

    task->extra.coordBody->coord->composeStamp = GRAPHICS_COORD_DIRTY;
    worldCollisionClearContacts(work->attackContacts);
    // Carry the attack body along with the blob.
    work->bodyCoord.coord.t[0]   = task->extra.coordBody->coord->coord.t[0];
    work->bodyCoord.coord.t[1]   = task->extra.coordBody->coord->coord.t[1];
    work->bodyCoord.coord.t[2]   = task->extra.coordBody->coord->coord.t[2];
    work->bodyCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->bodyCoord);
}
