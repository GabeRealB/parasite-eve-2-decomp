/* Part of the Glutton library; see glutton.h. */

/// Settling state that follows the bounce: the step counter drives the whole
/// thing. When the dispatcher flags a state change the horizontal velocity is
/// cut to a ninth, both light modes are reset and the two display nodes drop
/// the draw flags the descent raised. Past x 0x4B65 in room 0x0427 the x
/// velocity is killed outright; for the first eight steps what is left of it
/// moves the model and is halved again each step. Steps 1, 2, 4, 8 and 20 puff
/// a `0x600A5` effect out of the model's coordinate, and 4 and 8 also switch
/// the light mode. After 0x51 steps both nodes are unlinked and the task steps
/// on; until then the two collision-record tables are wiped each step. The
/// model's own `workm` translation is handed to `Gp_UpdateActorColor`.
void gluttonChunkSettle(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work = task->work;
    VECTOR                 pos;
    s16                    step;

    if (gGluttonEnded == 1) {
        Gp_UnlinkObj(&work->attackBody);
        Gp_UnlinkObj(&work->gridBody);
        enemyDestroy(enemy, task);
        return;
    }

    if (work->stateChanged != 0) {
        work->stateTicks     = 0;
        work->aim.travel.vx /= 9;
        work->aim.travel.vz /= 9;
        Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
        Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
        work->gridBody.flags   &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        work->attackBody.flags &= ~WORLD_COLLISION_BODY_PAIR_ENABLED;
        task->extra.tmd->flags  = TMD_OBJECT_SEMI_TRANS;
    }

    work->stateTicks++;

    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 39, 0, 0) &&
        task->extra.tmd->coords->coord.t[0] >= 0x4B65) {
        work->aim.travel.vx = 0;
    }

    if (work->stateTicks < 8) {
        task->extra.tmd->coords->coord.t[0]  += work->aim.travel.vx;
        task->extra.tmd->coords->coord.t[2]  += work->aim.travel.vz;
        work->aim.travel.vx                 >>= 1;
        work->aim.travel.vz                 >>= 1;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }

    step = work->stateTicks - 1;
    switch (step) {
        case 3:
        case 7:
            Gp_SpawnEff(EFFECT_CORPSE_BURN, task->extra.tmd->coords, 1, NULL);
            Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
            break;
        case 0:
        case 1:
        case 19:
            Gp_SpawnEff(EFFECT_CORPSE_BURN, task->extra.tmd->coords, 1, NULL);
            break;
    }

    if (work->stateTicks >= 0x51) {
        Gp_UnlinkObj(&work->attackBody);
        Gp_UnlinkObj(&work->gridBody);
        task->state++;
    }

    if (work->stateTicks < 0x51) {
        Gp_ClearRec18Occupied(work->gridContacts);
        Gp_ClearRec18Occupied(work->attackContacts);
    }

    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}
