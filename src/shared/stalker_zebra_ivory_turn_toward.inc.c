/* Part of the Zebra and Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Turns the actor's yaw (`yaw`) by `step` toward the world point
/// `target`, of which only `vx` and `vz` are read, leaving it alone while the
/// heading error is within 0x100. Clears the model root's `composeStamp` first so the
/// root is recomputed.
void stalkerZebraIvoryTurnToward(Task* arg0, SVECTOR* target, s32 step)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;
    GfxCoord*              coords;
    SVECTOR                vec;
    s32                    diff;
    s32                    yaw;
    u16                    angle;

    coords               = arg0->extra.tmd->coords;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    vec.vx               = target->vx - coords->coord.t[0];
    vec.vy               = 0;
    vec.vz               = target->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->yaw;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->yaw = angle - step;
    } else if (diff < -0x100) {
        work->yaw = angle + step;
    }
}
