/* Part of the footstep walk library; see footstep_walk.h. */

/// Placement handler: turns the model to the placement's yaw, keeping that yaw
/// in the work block, and moves it to the placement's position. Only the Y
/// rotation is applied.
s32 footstepWalkPlace(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord* coord;
    u16       yaw;

    coord                     = task->extra.tmd->coords;
    gFootstepWalkWork->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
