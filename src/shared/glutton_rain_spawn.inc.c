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
static __inline__ void _gluttonLinkRainAttackSphere(GluttonProjectileWork* work, const SVECTOR* localCenter, u16 radius)
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

    if (gGluttonEnded == 1) {
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
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        patternRoll     = (gRandomLcgState >> 16) & 3;
        switch (patternRoll) {
            case 0:
            case 1:
                gGluttonRainGroup = 1;
                break;
            case 2:
                gGluttonRainGroup = patternRoll;
                break;
            case 3:
                gGluttonRainGroup = 0;
                break;
            default:
                gGluttonRainGroup = 0;
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
        gGluttonRainPoints[gGluttonRainPointIndex[gGluttonRainGroup]
                                                 [(u16)task->spawnArg1.value]]
            .vz;
    work->aim.landing.vy = 0;
    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->aim.landing.vz = gGluttonRainPoints[gGluttonRainPointIndex[gGluttonRainGroup]
                                                                    [(u16)task->spawnArg1.value]]
                               .vx -
                           GLUTTON_RAIN_PATTERN_Z_OFFSET;
    work->aim.landing.vz = ((gRandomLcgState >> 16) & GLUTTON_RAIN_LANDING_JITTER_MASK) + work->aim.landing.vz;

    if ((u16)task->spawnArg1.value == GLUTTON_RAIN_PLAYER_TARGET_INDEX) {
        work->aim.landing.vx = playerTask->extra.tmd->coords->coord.t[0];
        work->aim.landing.vy = 0;
        work->aim.landing.vz = playerTask->extra.tmd->coords->coord.t[2];
    }

    work->stateTicks  = 0;
    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->speedJitter = (gRandomLcgState >> 16) & GLUTTON_RAIN_SPEED_JITTER_MASK;

    offset.vx = gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vx;
    offset.vy = gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vy;
    offset.vz = gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vz;

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
