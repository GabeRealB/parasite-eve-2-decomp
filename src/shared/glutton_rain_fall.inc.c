/* Part of the Glutton library; see glutton.h. */

/// Descent state of the enemy dispatched through `D_actor_444000_80131F1C`:
/// draw the growing shadow marker on the floor under the model, then after
/// 0x14 steps start pulling the model down by `0x258 + speedJitter` a step.
/// When it reaches floor level, zero the height, restart `stateTicks`, switch
/// `rainEffect` to its landing burst, play the landing cue and step the task
/// on. Either way `bodyCoord` is left tracking the model. Bails to
/// `enemyDestroy` when the overlay is shutting down.
void gluttonRainFall(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work;
    GluttonCoord           coord;
    Enemy*                 owner;
    s32                    snd;
    s32                    pan;

    work = task->work;
    if (gGluttonEnded == 1) {
        worldCollisionUnlinkBody(&work->attackBody);
        enemyDestroy(enemy, task);
        return;
    }

    work->stateTicks++;
    coord.node.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&coord.node.coord);
    gfxRotMatrixY(&coord.node.coord, 0, 1);

    coord.node.coord.t[0]   = task->extra.tmd->coords->coord.t[0];
    coord.node.coord.t[1]   = 0;
    coord.node.coord.t[2]   = task->extra.tmd->coords->coord.t[2];
    coord.node.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&coord.node);

    // The shadow starts spreading 0x14 ticks before the blob comes down.
    effectDrawGroundShadow(MATRIX_TRANS(&coord.node.workm), (s16)(work->stateTicks * 8 + 0x80),
                           gRoomEffectState->groundShadowShade);

    if (work->stateTicks >= 0x14) {
        task->extra.tmd->coords->coord.t[1] =
            task->extra.tmd->coords->coord.t[1] + (work->speedJitter + 0x258);
        if (task->extra.tmd->coords->coord.t[1] > 0) {
            owner                               = task->parent->spawnArg2.pointer;
            task->extra.tmd->coords->coord.t[1] = 0;
            work->stateTicks                    = 0;
            if (work->rainEffect != NULL) {
                work->rainEffect->task->spawnArg1.value = 2;
            }
            task->state++;
            snd = ((owner->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000C;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
    }

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    worldCollisionClearContacts(work->attackContacts);
    // Carry the attack body along with the blob.
    work->bodyCoord.node.coord.t[0]   = task->extra.tmd->coords->coord.t[0];
    work->bodyCoord.node.coord.t[1]   = task->extra.tmd->coords->coord.t[1];
    work->bodyCoord.node.coord.t[2]   = task->extra.tmd->coords->coord.t[2];
    work->bodyCoord.node.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->bodyCoord.node);
}
