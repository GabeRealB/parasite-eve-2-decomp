/* Part of the Glutton library; see glutton.h. */

/// Per-tick state of the spinner enemy. While `chaseDelay` is counting down the
/// model only yaws in place -- 0x40 on phase 1 and -0x3C on phase 3 of every four
/// frames -- and nothing else happens. Once it reaches zero the enemy homes on
/// `gGluttonSpinnerTarget`: the offset from the model root to that point is
/// squared against `chaseSpeed` in a `VECTOR3` borrowed off the scratch stack,
/// and the task steps on when the enemy is inside that radius. `chaseTicks` then
/// counts the step and accelerates the flight (`chaseSpeed += chaseTicks / 8`),
/// the offset is normalised and scaled by `chaseSpeed` through the GTE's `gpf`
/// interpolator, and the result is added to the root translation before the
/// model is turned about its three axes by angles taken from `chaseSpeed` and
/// `chaseTicks`. Bails to `enemyDestroy` while the overlay is shutting down.
void gluttonSpinnerChase(Enemy* enemy, Task* task)
{
    GluttonSpinnerWork* work;
    SVECTOR             step;
    SVECTOR*            stepp;
    VECTOR3*            sq;
    u8*                 head;
    s16                 speed;
    s32                 delay;
    s32                 phase;
    s32                 inside;

    work                                  = task->work;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(task->extra.tmd->coords);
    func_800D7A9C(task->extra.tmd, (VECTOR*)task->extra.tmd->coords->workm.t, 0, 3);

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
            gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x40, 0);
        }
        if ((work->chaseDelay & 3) == 3) {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, -0x3C, 0);
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(task->extra.tmd->coords);
        return;
    }

    stepp    = &step;
    *stepp   = gGluttonSpinnerTarget;
    step.vx -= task->extra.tmd->coords->coord.t[0];
    step.vy -= task->extra.tmd->coords->coord.t[1];
    step.vz -= task->extra.tmd->coords->coord.t[2];

    head = actorGetScratchHead();
    sq   = (VECTOR3*)(head - sizeof(VECTOR3));
    actorSetScratchHead(sq);
    speed  = work->chaseSpeed;
    sq->vx = step.vx;
    sq->vy = stepp->vz;
    sq->vz = speed;
    sq->vx = sq->vx * sq->vx;
    sq->vy = sq->vy * sq->vy;
    sq->vz = sq->vz * sq->vz;
    actorSetScratchHead(head);
    inside = sq->vx + sq->vy >= sq->vz;
    if (!inside) {
        task->state++;
    }

    work->chaseTicks++;
    work->chaseSpeed += work->chaseTicks / 8;
    VectorNormalSS(stepp, stepp);

    gte_lddp((u16)work->chaseSpeed);
    gte_ldsv(stepp);
    gte_gpf12();
    gte_stsv(stepp);

    task->extra.tmd->coords->coord.t[0]  += step.vx;
    task->extra.tmd->coords->coord.t[1]  += step.vy;
    task->extra.tmd->coords->coord.t[2]  += step.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->chaseSpeed / 2, 0);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, work->chaseSpeed * 2, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, work->chaseTicks, GRAPHICS_ROTATION_COMPOSE);
}
