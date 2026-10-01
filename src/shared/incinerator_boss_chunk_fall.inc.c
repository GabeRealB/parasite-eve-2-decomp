/* Part of the incinerator boss library; see incinerator_boss.h. */

/// Descent state that follows the hold: once the model's y has passed its apex
/// (gone negative) both display nodes get their draw flags raised and the
/// bounce height `field_1AA` is added back to y as a magnitude each step. When
/// y reaches -0x31 or above it is clamped to -0x32, the step counter is reset,
/// the landing sound is played at the model's own pan and depth, and the task
/// steps on. Collision against `rec1` -- and, in room 0x0427 past x 0x4B65 --
/// kills the horizontal velocity, whatever is left of it moves the model by a
/// ninth per step, and the model's own `workm` translation is handed to
/// `Gp_UpdateActorColor`.
void incinBossChunkFall(Enemy* enemy, Task* task)
{
    Actor403200GrabWork* work = task->work;
    VECTOR               pos;
    s32                  pan;

    if (gIncinBossEnded == 1) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
    }

    if (task->extra.tmd->coords->coord.t[1] < 0) {
        work->obj0.flags                    |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->obj1.flags                    |= WORLD_COLLISION_BODY_GRID_ENABLED;
        task->extra.tmd->coords->coord.t[1] += ABS(work->field_1AA);
    }

    if (task->extra.tmd->coords->coord.t[1] >= -0x31) {
        task->extra.tmd->coords->coord.t[1] = -0x32;
        work->field_1AC                     = 0;
        pan                                 = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(0x4020000C, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        task->state++;
    }

    if (ActorContact_PushContact(task->extra.tmd->coords, &work->rec1, 3) != 0) {
        work->vel.vz = 0;
        work->vel.vx = 0;
    }

    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 39, 0, 0) &&
        task->extra.tmd->coords->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    Gp_ClearRec18Occupied(&work->rec1);
    Gp_ClearRec18Occupied(&work->rec0);

    task->extra.tmd->coords->coord.t[0]  += work->vel.vx / 9;
    task->extra.tmd->coords->coord.t[2]  += work->vel.vz / 9;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}
