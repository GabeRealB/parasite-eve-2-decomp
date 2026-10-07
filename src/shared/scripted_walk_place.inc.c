/* Part of the scripted walk library; see scripted_walk.h. */

/// Places the receiver's model root at a position and yaw in its parent's space.
///
/// Handles `ACTOR_MESSAGE_PLACE` for a live TMD task with its own live work
/// block selected by `SCRIPTED_WALK_WORK`. The borrowed, word-aligned placement
/// supplies XYZ in whole parent-coordinate units and signed yaw in 1/4096 turns;
/// pitch, roll and the vectors' fourth components are ignored. Retains the yaw
/// in `st.yaw`, replaces the root rotation at unit scale and invalidates
/// composition. Returns zero; `messageId` and `unusedArgument` are ignored.
/// The placement pointer is consumed during dispatch and is not retained.
s32 SCRIPTED_WALK_PLACE(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument)
{
    GfxCoord* rootCoord;
    s16       yaw;

    rootCoord                  = task->extra.tmd->coords;
    SCRIPTED_WALK_WORK->st.yaw = yaw = placement->rot.vy;
    // Replace pitch, roll and scale while preserving the root's parent.
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}
