/* Part of the paced walk library; see paced_walk.h. */

/// Script opcode "place at": yaws the actor's root coordinate to
/// `placement->rot.vy`, caching that yaw in the work block, then drops the
/// placement translation into the matrix and marks it dirty.
s32 pacedWalkPlace(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord*          coord;
    PACED_WALK_WORK_T* work;
    u16                yaw;

    coord        = task->extra.tmd->coords;
    work         = task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    gfxRotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
