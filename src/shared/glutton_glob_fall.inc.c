/* Part of the Glutton library; see glutton.h. */

/// Death throes of the grabbing enemy: bounce the model on the floor until it
/// settles. While the model is still below the floor plane (`coord.t[1] > 0`)
/// it is snapped back to -0x32, the step counter is cleared, the impact cue is
/// enqueued with the object's own pan and half its depth, and the task steps
/// on. Otherwise the body keeps falling by `fallStep`'s magnitude, drifts a
/// fifteenth of `aim.travel` in x and z, has its colour refreshed from the model's
/// world position, damps the two shake terms and has its rotation rebuilt at
/// half scale.
void gluttonGlobFall(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work = task->work;
    GfxCoord*              coord;
    VECTOR                 pos;
    s32                    sfx;
    s32                    pan;
    s32                    drop;
    s32                    bounce;

    if (gGluttonEnded == 1) {
        enemyDestroy(enemy, task);
        return;
    }

    coord = task->extra.tmd->coords;
    drop  = coord->coord.t[1];
    if (drop > 0) {
        coord->coord.t[1] = -0x32;
        work->stateTicks  = 0;
        sfx               = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000C;
        pan               = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
        task->state++;
        return;
    }

    bounce            = ABS(work->fallStep);
    coord->coord.t[1] = drop + bounce;

    task->extra.tmd->coords->coord.t[0]  += work->aim.travel.vx / 15;
    task->extra.tmd->coords->coord.t[2]  += work->aim.travel.vz / 15;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;

    gluttonShrinkRotation(task->extra.tmd->coords);
}
