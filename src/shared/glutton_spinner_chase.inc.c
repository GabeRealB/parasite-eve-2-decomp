/* Part of the Glutton library; see glutton.h. */

/// Tests the spinner's horizontal offset against its current movement radius.
///
/// Needs twelve free scratch bytes and a squared XZ sum that fits s32.
/// Restores before the final comparison, with no call that could reuse the block.
static inline s32 _gluttonSpinnerOutsideArrivalRadius(const SVECTOR* offset, const GluttonSpinnerWork* work)
{
    OverlayRangeScratch* rangeSquares;
    OverlayRangeScratch* scratchEnd;
    s16                  speed;

    scratchEnd   = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    rangeSquares = scratchEnd - 1;
    _scratchStackSetCursor(rangeSquares);
    speed                 = work->chaseSpeed;
    rangeSquares->dx      = offset->vx;
    rangeSquares->dz      = offset->vz;
    rangeSquares->radius  = speed;
    rangeSquares->dx     *= rangeSquares->dx;
    rangeSquares->dz     *= rangeSquares->dz;
    rangeSquares->radius *= rangeSquares->radius;
    _scratchStackSetCursor(scratchEnd);
    // No reservation can intervene between restore and the final read.
    return rangeSquares->dx + rangeSquares->dz >= rangeSquares->radius;
}

/// Delays a released Glutton spinner, then accelerates it toward its target.
///
/// Requires initialized spinner work, a live model with writable lighting
/// matrices and a target in the root's parent frame. Offsets narrow to s16;
/// distance and speed use game units, angles use 4096 units per turn. Needs a
/// free word-aligned twelve-byte scratch block and initialized GTE state.
/// Advances only when XZ distance is strictly below the pre-acceleration speed;
/// equality keeps chasing, and the arrival tick still accelerates and moves.
/// Encounter shutdown or a cleared release flag destroys the spinner.
static void _gluttonSpinnerChase(Enemy* enemy, Task* task)
{
    GluttonSpinnerWork* work;
    SVECTOR             targetOffset;
    SVECTOR*            movement;
    s32                 delay;
    s32                 phase;
    s32                 outsideArrivalRadius;

    work                                  = task->work;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldCoordSetModelLighting(task->extra.tmd, task->extra.tmd->coords->workm.t, 0, 3);

    if (gGluttonEnded == 1 || gGluttonSpinnersReleased == 0) {
        enemyDestroy(enemy, task);
        return;
    }

    task->extra.tmd->flags = 0;

    delay = work->chaseDelay;
    if (delay != 0) {
        delay--;
        work->chaseDelay = delay;
        phase            = work->chaseDelay;
        if ((phase & 3) == 1) {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x40, GRAPHICS_ROTATION_COMPOSE);
        }
        if ((work->chaseDelay & 3) == 3) {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, -0x3C, GRAPHICS_ROTATION_COMPOSE);
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
        return;
    }

    movement         = &targetOffset;
    *movement        = gGluttonSpinnerTarget;
    targetOffset.vx -= task->extra.tmd->coords->coord.t[0];
    targetOffset.vy -= task->extra.tmd->coords->coord.t[1];
    targetOffset.vz -= task->extra.tmd->coords->coord.t[2];

    outsideArrivalRadius = _gluttonSpinnerOutsideArrivalRadius(movement, work);
    if (!outsideArrivalRadius) {
        task->state++;
    }

    // Arrival still completes this tick's acceleration, displacement and spin.
    work->chaseTicks++;
    work->chaseSpeed += work->chaseTicks / 8;
    VectorNormalSS(movement, movement);

    gte_lddp((u16)work->chaseSpeed);
    gte_ldsv(movement);
    gte_gpf12();
    gte_stsv(movement);

    task->extra.tmd->coords->coord.t[0]  += targetOffset.vx;
    task->extra.tmd->coords->coord.t[1]  += targetOffset.vy;
    task->extra.tmd->coords->coord.t[2]  += targetOffset.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->chaseSpeed / 2, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, work->chaseSpeed * 2, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, work->chaseTicks, GRAPHICS_ROTATION_COMPOSE);
}
