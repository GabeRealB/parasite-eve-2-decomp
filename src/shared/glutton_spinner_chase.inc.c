/* Part of the Glutton library; see glutton.h. */

/// Per-tick state of the spinner enemy. While `spin` is counting down the model
/// only yaws in place -- 0x40 on phase 1 and -0x3C on phase 3 of every four
/// frames -- and nothing else happens. Once it reaches zero the enemy homes on
/// `gGluttonSpinnerTarget`: the offset from the model root to that point is
/// squared against `field_98` in a `VECTOR3` borrowed off the scratch stack, and
/// the task steps on when the enemy is inside that radius. `field_96` then ties
/// the spin rate to the step count (`field_98 += field_96 / 8`), the offset is
/// normalised and scaled by `field_98` through the GTE's `gpf` interpolator, and
/// the result is added to the root translation before the three rotations are
/// rebuilt from `field_98` and `field_96`. Bails to `Gp_DestroyEnemy` while the
/// overlay is shutting down.
void gluttonSpinnerChase(Enemy* enemy, Task* task)
{
    GluttonSpinnerWork* work;
    SVECTOR             step;
    SVECTOR*            stepp;
    VECTOR3*            sq;
    u8*                 head;
    s16                 angle;
    s32                 spin;
    s32                 phase;
    s32                 inside;

    work                                  = task->work;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(task->extra.tmd->coords);
    func_800D7A9C(task->extra.tmd, (VECTOR*)task->extra.tmd->coords->workm.t, 0, 3);

    if (gGluttonEnded == 1 || gGluttonSpinnersReleased == 0) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    task->extra.tmd->flags = 0;

    spin = work->spin;
    if (spin != 0) {
        spin--;
        work->spin = spin;
        phase      = work->spin;
        if ((phase & 3) == 1) {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x40, 0);
        }
        if ((work->spin & 3) == 3) {
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
    angle  = work->field_98;
    sq->vx = step.vx;
    sq->vy = stepp->vz;
    sq->vz = angle;
    sq->vx = sq->vx * sq->vx;
    sq->vy = sq->vy * sq->vy;
    sq->vz = sq->vz * sq->vz;
    actorSetScratchHead(head);
    inside = sq->vx + sq->vy >= sq->vz;
    if (!inside) {
        task->state++;
    }

    work->field_96++;
    work->field_98 += work->field_96 / 8;
    VectorNormalSS(stepp, stepp);

    gte_lddp((u16)work->field_98);
    gte_ldsv(stepp);
    gte_gpf12();
    gte_stsv(stepp);

    task->extra.tmd->coords->coord.t[0]  += step.vx;
    task->extra.tmd->coords->coord.t[1]  += step.vy;
    task->extra.tmd->coords->coord.t[2]  += step.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->field_98 / 2, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, work->field_98 * 2, 0);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, work->field_96, GRAPHICS_ROTATION_COMPOSE);
}
