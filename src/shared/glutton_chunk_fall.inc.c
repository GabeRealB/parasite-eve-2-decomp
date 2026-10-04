/* Part of the Glutton library; see glutton.h. */

/// Descent state that follows the hold: once the model's y has passed its apex
/// (gone negative) both display nodes get their draw flags raised and the
/// bounce height `fallStep` is added back to y as a magnitude each step. When
/// y reaches -0x31 or above it is clamped to -0x32, the step counter is reset,
/// the landing sound is played at the model's own pan and depth, and the task
/// steps on. Collision against `gridContacts` -- and, in room 0x0427 past x 0x4B65 --
/// kills the horizontal velocity, whatever is left of it moves the model by a
/// ninth per step, and the model's own `workm` translation is handed to
/// `Gp_UpdateActorColor`.
void gluttonChunkFall(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work = task->work;
    VECTOR                 pos;
    s32                    pan;

    if (gGluttonEnded == 1) {
        Gp_UnlinkObj(&work->attackBody);
        Gp_UnlinkObj(&work->gridBody);
        enemyDestroy(enemy, task);
        return;
    }

    if (work->stateChanged != 0) {
        Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
    }

    if (task->extra.tmd->coords->coord.t[1] < 0) {
        work->attackBody.flags              |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->gridBody.flags                |= WORLD_COLLISION_BODY_GRID_ENABLED;
        task->extra.tmd->coords->coord.t[1] += ABS(work->fallStep);
    }

    if (task->extra.tmd->coords->coord.t[1] >= -0x31) {
        task->extra.tmd->coords->coord.t[1] = -0x32;
        work->stateTicks                    = 0;
        pan                                 = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(SOUND_GLUTTON_CHUNK_LAND, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        task->state++;
    }

    if (ActorContact_PushContact(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 0) {
        work->aim.travel.vz = 0;
        work->aim.travel.vx = 0;
    }

    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 39, 0, 0) &&
        task->extra.tmd->coords->coord.t[0] >= 0x4B65) {
        work->aim.travel.vx = 0;
    }

    Gp_ClearRec18Occupied(work->gridContacts);
    Gp_ClearRec18Occupied(work->attackContacts);

    task->extra.tmd->coords->coord.t[0]  += work->aim.travel.vx / 9;
    task->extra.tmd->coords->coord.t[2]  += work->aim.travel.vz / 9;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}
