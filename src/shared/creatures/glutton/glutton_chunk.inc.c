#include "main/random.h"
#include "main/sound_ids.h"

/* Part of the Glutton library; see glutton.h. */

/// Links the debris chunk's independent room-grid sphere and its contact table.
///
/// Requires an unlinked body and a nonnegative radius in world units. The work
/// and borrowed coordinate must stay live until unlink. Its centre is the
/// coordinate's origin and its enemy-body identity is 10. Linking leaves grid
/// tests disabled; the caller initializes all three contacts before enabling
/// them. Assigning sphere flags replaces any previous test flags.
static inline void _gluttonLinkChunkGridSphere(GluttonProjectileWork* work, GfxCoord* coord, s16 radius)
{
    enum { GLUTTON_CHUNK_GRID_BODY_ID = 10 };
    work->gridBody.coord            = coord;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | GLUTTON_CHUNK_GRID_BODY_ID;
    work->gridBody.radius           = radius;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
}

/// Creates a falling debris chunk at the world origin of the host's part 3.
///
/// Requires a live TMD body, parent host with part 3 and six attack entries,
/// and player TMD coordinate in the view frame. Allocates zeroed projectile
/// work owned by the task, links its attack and grid spheres, and lends its
/// lighting matrices to the model. Travel is the player's launch-time X/Z
/// offset in world units, spent over nine descent ticks. Shutdown or allocation
/// failure destroys the enemy; success advances to descent.
static void _gluttonChunkSpawn(Enemy* enemy, Task* task)
{
    enum { GLUTTON_CHUNK_FALL_TICKS      = 9,
           GLUTTON_CHUNK_RADIUS          = 256,
           GLUTTON_CHUNK_ATTACK_INDEX    = 5,
           GLUTTON_CHUNK_LAUNCH_YAW_MASK = 511,
           GLUTTON_CHUNK_LAUNCH_SOUND    = SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0B) };
    GluttonProjectileWork* work;
    Enemy*                 hostEnemy;
    Task*                  playerTask;
    SVECTOR                launchPoint;
    s32                    soundId;
    s32                    audioPan;

    hostEnemy  = task->parent->spawnArg2.pointer;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED ||
        (work = memCalloc(sizeof(GluttonProjectileWork), false), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }

    task->extra.tmd->coords->parent = &gGfxViewCoord;
    task->extra.tmd->flags          = 0;

    // Resolve the host part's origin before moving the chunk into the view frame.
    launchPoint.vx = launchPoint.vy = launchPoint.vz = 0;
    _actorRenderTransformLocalPointToWorld(&hostEnemy->task->extra.tmd->coords[3], &launchPoint);

    task->extra.tmd->coords->coord.t[0]   = launchPoint.vx;
    task->extra.tmd->coords->coord.t[1]   = launchPoint.vy;
    task->extra.tmd->coords->coord.t[2]   = launchPoint.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    work->fallStep = task->extra.tmd->coords->coord.t[1] / GLUTTON_CHUNK_FALL_TICKS;
    work->aim.travel.vx =
        playerTask->extra.tmd->coords->coord.t[0] - task->extra.tmd->coords->coord.t[0];
    work->aim.travel.vy = 0;
    work->aim.travel.vz =
        playerTask->extra.tmd->coords->coord.t[2] - task->extra.tmd->coords->coord.t[2];
    work->stateTicks = 0;
    task->state++;

    soundId  = ((hostEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | GLUTTON_CHUNK_LAUNCH_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));

    RANDOM_LCG_ADVANCE();
    gfxRotMatrixY(&task->extra.tmd->coords->coord, RANDOM_LCG_DRAW() & GLUTTON_CHUNK_LAUNCH_YAW_MASK, GRAPHICS_ROTATION_REPLACE);

    launchPoint.vx = launchPoint.vy = launchPoint.vz = 0;

    // Attack contacts damage the player; the separate grid sphere stops the slide.
    _worldCollisionLinkSphereBody(task->extra.tmd->coords, &work->attackBody, work->attackContacts, &launchPoint, GLUTTON_CHUNK_RADIUS, WORLD_COLLISION_LIST_ENEMY_ATTACKS,
                                  ARRAY_SIZE(work->attackContacts));

    _gluttonLinkChunkGridSphere(work, task->extra.tmd->coords, GLUTTON_CHUNK_RADIUS);

    work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    work->attackBody.key  = damagePackEnemyAttackKey(hostEnemy, GLUTTON_CHUNK_ATTACK_INDEX);

    task->extra.tmd->lightMtx = &work->lightMtx;
    task->extra.tmd->colorMtx = &work->colorMtx;
}

/// Drops a debris chunk onto the floor while spending its horizontal travel.
///
/// Enables attack and grid tests below the launch point and descends by the
/// magnitude of `fallStep` each tick. At y >= -49 it clamps to -50, plays the
/// landing cue and advances to settling. A grid collision stops both horizontal
/// components; the dumping-hole arena's x limit stops only x. Translation uses
/// view-coordinate world units and one ninth of the launch-to-player offset.
/// Encounter shutdown unlinks both bodies and destroys the enemy immediately.
/// Requires the live chunk's TMD body and initialized `GluttonProjectileWork`.
static void _gluttonChunkFall(Enemy* enemy, Task* task)
{
    enum { GLUTTON_CHUNK_FLOOR_Y = -50 };
    GluttonProjectileWork* work = task->work;
    VECTOR                 worldPosition;
    s32                    audioPan;

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
        worldCollisionUnlinkBody(&work->attackBody);
        worldCollisionUnlinkBody(&work->gridBody);
        enemyDestroy(enemy, task);
        return;
    }

    if (work->stateChanged != 0) {
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
    }

    if (task->extra.tmd->coords->coord.t[1] < 0) {
        work->attackBody.flags              |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->gridBody.flags                |= WORLD_COLLISION_BODY_GRID_ENABLED;
        task->extra.tmd->coords->coord.t[1] += ABS(work->fallStep);
    }

    if (task->extra.tmd->coords->coord.t[1] > GLUTTON_CHUNK_FLOOR_Y) {
        task->extra.tmd->coords->coord.t[1] = GLUTTON_CHUNK_FLOOR_Y;
        work->stateTicks                    = 0;
        audioPan                            = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(SOUND_GLUTTON_CHUNK_LAND, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        task->state++;
    }

    // A wall stops travel; contacts from this tick are then discarded.
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 0) {
        work->aim.travel.vz = 0;
        work->aim.travel.vx = 0;
    }

    if ((GAME_LOCATION_WORD(gGameSession->location) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 0, 0) &&
        task->extra.tmd->coords->coord.t[0] >= GLUTTON_CHUNK_DUMPING_HOLE_STOP_X) {
        work->aim.travel.vx = 0;
    }

    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->attackContacts);

    task->extra.tmd->coords->coord.t[0]  += work->aim.travel.vx / 9;
    task->extra.tmd->coords->coord.t[2]  += work->aim.travel.vz / 9;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);
}
