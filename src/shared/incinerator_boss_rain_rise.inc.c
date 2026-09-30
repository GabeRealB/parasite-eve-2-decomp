/* Part of the incinerator boss library; see incinerator_boss.h. */

/// Ascent state that precedes the descent above: lift the model by 0x1F4 plus
/// `field_1AE` a step until it passes -0x4E20, then clamp it there, snap its
/// horizontal position back onto `work->target`, restart the step counter, pick
/// a fresh 0..0x1F bias for the next leg, flag the list object and step the task
/// on. Either way the work block's own coordinate is left tracking the model.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down.
void incinBossRainRise(Enemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    s32                  y;

    work = task->work;
    if (gIncinBossEnded == 1) {
        Gp_UnlinkObj(&work->obj);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    y                                   = task->extra.tmd->coords->coord.t[1] - 0x1F4;
    task->extra.tmd->coords->coord.t[1] = y - work->field_1AE;
    if (task->extra.tmd->coords->coord.t[1] < -0x4E20) {
        task->state++;
        task->extra.tmd->coords->coord.t[0] = work->target.vx;
        task->extra.tmd->coords->coord.t[2] = work->target.vz;
        Gp_LcgState                         = Gp_LcgState * 5 + 0x71357911;
        task->extra.tmd->coords->coord.t[1] = -0x4E20;
        work->timer                         = 0;
        work->field_1AE                     = ((u32)Gp_LcgState >> 16) & 0x1F;
        work->obj.flags                    |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->coord.coord.t[0]                = task->extra.tmd->coords->coord.t[0];
    work->coord.coord.t[1]                = task->extra.tmd->coords->coord.t[1];
    work->coord.coord.t[2]                = task->extra.tmd->coords->coord.t[2];
    work->coord.composeStamp              = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&work->coord);
}
