/* Part of the Glutton library; see glutton.h. */

/// Slides and darkens a landed debris chunk before advancing to teardown.
///
/// Entry disables both collision tests and reduces travel to one ninth. Ticks
/// 1 through 7 move it and halve that travel; the dumping-hole arena's x limit
/// stops x. Burn effects appear on ticks 1, 2, 4, 8 and 20, with black lighting
/// from tick 4. Tick 81 unlinks both bodies and advances to teardown. Contacts
/// are cleared only before that tick. Shutdown destroys the enemy immediately.
/// Requires the live chunk's TMD body and initialized `GluttonProjectileWork`.
static void _gluttonChunkSettle(Enemy* enemy, Task* task)
{
    enum { GLUTTON_CHUNK_SETTLE_TICKS = 81 };
    GluttonProjectileWork* work = task->work;
    VECTOR                 worldPosition;
    s16                    settleTickIndex;

    if (gGluttonEnded == 1) {
        worldCollisionUnlinkBody(&work->attackBody);
        worldCollisionUnlinkBody(&work->gridBody);
        enemyDestroy(enemy, task);
        return;
    }

    if (work->stateChanged != 0) {
        work->stateTicks     = 0;
        work->aim.travel.vx /= 9;
        work->aim.travel.vz /= 9;
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
        work->gridBody.flags   &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        work->attackBody.flags &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
        task->extra.tmd->flags  = TMD_OBJECT_SEMI_TRANS;
    }

    work->stateTicks++;

    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_DUMPING_HOLE, 0, 0) &&
        task->extra.tmd->coords->coord.t[0] >= GLUTTON_CHUNK_DUMPING_HOLE_STOP_X) {
        work->aim.travel.vx = 0;
    }

    if (work->stateTicks < 8) {
        task->extra.tmd->coords->coord.t[0]  += work->aim.travel.vx;
        task->extra.tmd->coords->coord.t[2]  += work->aim.travel.vz;
        work->aim.travel.vx                 >>= 1;
        work->aim.travel.vz                 >>= 1;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }

    settleTickIndex = work->stateTicks - 1;
    switch (settleTickIndex) {
        case 3:
        case 7:
            effectSpawn(EFFECT_CORPSE_BURN, task->extra.tmd->coords, 1, NULL);
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            break;
        case 0:
        case 1:
        case 19:
            effectSpawn(EFFECT_CORPSE_BURN, task->extra.tmd->coords, 1, NULL);
            break;
    }

    if (work->stateTicks >= GLUTTON_CHUNK_SETTLE_TICKS) {
        worldCollisionUnlinkBody(&work->attackBody);
        worldCollisionUnlinkBody(&work->gridBody);
        task->state++;
    }

    if (work->stateTicks < GLUTTON_CHUNK_SETTLE_TICKS) {
        worldCollisionClearContacts(work->gridContacts);
        worldCollisionClearContacts(work->attackContacts);
    }

    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);
}
