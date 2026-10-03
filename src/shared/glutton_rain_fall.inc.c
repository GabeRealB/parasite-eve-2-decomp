/* Part of the Glutton library; see glutton.h. */

/// Descent state of the enemy dispatched through `D_actor_444000_80131F1C`:
/// draw the growing shadow marker on the floor under the model, then after
/// 0x14 steps start pulling the model down by `0x258 + field_1AE` a step. When
/// it reaches floor level, zero the height, restart the step counter, tell the
/// trailing `Gp_SpawnEff` effect to wind down, play the landing cue and step
/// the task on. Either way the work block's own coordinate is left tracking
/// the model. Bails to `enemyDestroy` when the overlay is shutting down.
void gluttonRainFall(Enemy* enemy, Task* task)
{
    GluttonDropWork* work;
    GluttonCoord     coord;
    GfxMatrix*       mtx;
    Enemy*           owner;
    s32              snd;
    s32              pan;

    work = task->work;
    if (gGluttonEnded == 1) {
        Gp_UnlinkObj(&work->obj);
        enemyDestroy(enemy, task);
        return;
    }

    work->timer++;
    coord.node.parent                       = &gGfxViewCoord;
    mtx                                     = &coord.packed.coord;
    coord.packed.coord.rotationWords.m00M01 = ONE;
    coord.packed.coord.rotationWords.m02M10 = 0;
    mtx->rotationWords.m11M12               = ONE;
    coord.packed.coord.rotationWords.m20M21 = 0;
    mtx->rotationWords.m22                  = ONE;
    gfxRotMatrixY(&mtx->mat, 0, 1);

    coord.node.coord.t[0]   = task->extra.tmd->coords->coord.t[0];
    coord.node.coord.t[1]   = 0;
    coord.node.coord.t[2]   = task->extra.tmd->coords->coord.t[2];
    coord.node.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&coord.node);

    Gp_DrawEffGroundQuad(MATRIX_TRANS(&coord.node.workm), (s16)((s16)work->timer * 8 + 0x80),
                         gRoomEffectState->groundShadowShade);

    if ((s16)work->timer >= 0x14) {
        task->extra.tmd->coords->coord.t[1] =
            task->extra.tmd->coords->coord.t[1] + (work->field_1AE + 0x258);
        if (task->extra.tmd->coords->coord.t[1] > 0) {
            owner                               = task->parent->spawnArg2.pointer;
            task->extra.tmd->coords->coord.t[1] = 0;
            work->timer                         = 0;
            if (work->eff != NULL) {
                work->eff->task->spawnArg1.value = 2;
            }
            task->state++;
            snd = ((owner->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000C;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
    }

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_ClearRec18Occupied(&work->rec);
    work->coord.coord.t[0]   = task->extra.tmd->coords->coord.t[0];
    work->coord.coord.t[1]   = task->extra.tmd->coords->coord.t[1];
    work->coord.coord.t[2]   = task->extra.tmd->coords->coord.t[2];
    work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&work->coord);
}
