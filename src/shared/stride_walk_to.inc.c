/* Part of the stride walk library; see stride_walk.h. */

/// Script opcode "walk to": aims the actor's root coordinate at `target` by
/// taking the yaw of the horizontal offset from the coordinate's own
/// translation, caches that yaw in the work block and rebuilds the local
/// matrix from it, then records the distance, in steps of 30, for the walk
/// that follows.
s32 strideWalkTo(Task* task, s32 arg1, ActorTransform* target, s32 arg3)
{
    GfxCoord*       coord;
    StrideWalkWork* work;
    s32             dx;
    s32             dz;
    u16             yaw;

    coord        = task->extra.tmd->coords;
    work         = task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    gfxRotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 30;
    return 0;
}
