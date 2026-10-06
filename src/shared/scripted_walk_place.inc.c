/* Part of the scripted walk library; see scripted_walk.h. */

/// Message 0x7D4 (placement): turns the model to the placement's yaw, keeping
/// that yaw in the work block, and moves it to the placement's position. Only
/// the Y rotation is applied.
s32 scriptedWalkPlace(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GfxCoord* coord;
    u16       yaw;

    coord                      = task->extra.tmd->coords;
    SCRIPTED_WALK_WORK->st.yaw = yaw = placement->rot.vy;
    gfxRotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
