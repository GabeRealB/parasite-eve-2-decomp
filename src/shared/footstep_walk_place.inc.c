/* Part of the footstep walk library; see footstep_walk.h. */

/// Places the receiver's model root at a position and yaw in its parent's space.
///
/// Handles `ACTOR_MESSAGE_PLACE` for a live model task whose work block is
/// published in `gFootstepWalkWork`. The borrowed placement supplies xyz and
/// yaw in 1/4096 turns; pitch and roll are ignored. Stores the heading, replaces
/// the rotation at unit scale and invalidates composition. Returns zero;
/// `messageId` and `unusedArgument` are ignored and no payload pointer is retained.
static s32 _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument)
{
    GfxCoord* rootCoord;
    s16       yaw;

    rootCoord                 = task->extra.tmd->coords;
    gFootstepWalkWork->st.yaw = yaw = placement->rot.vy;
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
