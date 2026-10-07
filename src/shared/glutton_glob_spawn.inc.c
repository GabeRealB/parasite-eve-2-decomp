/* Part of the Glutton library; see glutton.h. */

/// Entry state of the enemy dispatched through `D_actor_444000_80131F0C`:
/// allocate its work block and drop the model onto the floor of the view
/// coordinate, under escort 1 of the host actor.
///
/// The model is reparented to `gGfxViewCoord`, its texture page cleared and its
/// CLUT row set to 2, and -- once the stream buffers exist -- processed twice
/// before the spawn cue is enqueued at the model's own pan and half its depth
/// with the owner's id in its high half. The task's light and colour matrices
/// are pointed into the work block, the translation is replaced by the world
/// position of part 1 of escort 1's model, and `fallStep` is a fifteenth of
/// that height. `aim.travel` is the horizontal gap to the player, which the later
/// states spend a fifteenth at a time. The rotation is finally rebuilt at half
/// scale around the yaw the model already faces.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void gluttonGlobSpawn(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work;
    Enemy*                 owner;
    GluttonWork*           host;
    Task*                  player;
    SVECTOR                vec;
    s32                    sfx;
    s32                    pan;

    owner  = task->parent->spawnArg2.pointer;
    host   = owner->task->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

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

    task->extra.tmd->coords->parent    = &gGfxViewCoord;
    task->extra.tmd->flags             = 0;
    task->extra.tmd->texturePageOffset = 0;
    task->extra.tmd->clutRowOffset     = 2;

    if (task->extra.tmd->buffer != NULL) {
        tmdBuildBufferHalf(task->extra.tmd);
        tmdBuildBufferHalf(task->extra.tmd);
        sfx = ((owner->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020001C;
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(sfx, pan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
    }

    task->extra.tmd->lightMtx = &work->lightMtx;
    task->extra.tmd->colorMtx = &work->colorMtx;

    vec.vx = vec.vy = vec.vz = 0;
    _actorRenderTransformLocalPointToWorld(&host->escorts[1]->task->extra.tmd->coords[1], &vec);

    task->extra.tmd->coords->coord.t[0]   = vec.vx;
    task->extra.tmd->coords->coord.t[1]   = vec.vy;
    task->extra.tmd->coords->coord.t[2]   = vec.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    work->fallStep = task->extra.tmd->coords->coord.t[1] / 15;
    work->aim.travel.vx =
        player->extra.tmd->coords->coord.t[0] - task->extra.tmd->coords->coord.t[0];
    work->aim.travel.vy = 0;
    work->aim.travel.vz =
        player->extra.tmd->coords->coord.t[2] - task->extra.tmd->coords->coord.t[2];
    work->stateTicks   = 0;
    work->playerCaught = 0;

    _actorRenderRescaleYawHalf(task->extra.tmd->coords);
    task->state++;
}
