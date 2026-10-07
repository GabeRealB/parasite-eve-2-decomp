/* Part of the Glutton library; see glutton.h. */

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

    if (gGluttonEnded == 1) {
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

    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 0, 0) &&
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
