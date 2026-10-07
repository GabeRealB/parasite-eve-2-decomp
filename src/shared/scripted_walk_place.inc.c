/* Part of the scripted walk library; see scripted_walk.h. */

#ifndef SRC_SHARED_SCRIPTED_WALK_PLACE_ROOT_COORD
#define SRC_SHARED_SCRIPTED_WALK_PLACE_ROOT_COORD
/// Replaces a model root's parent-space transform with a yaw and XYZ placement.
///
/// Borrows a writable root and readable placement through the call; yaw uses
/// 4096 units per turn and XYZ uses whole parent-coordinate units. Replaces
/// pitch, roll and scale, preserves the parent and invalidates composition.
static __inline__ void _scriptedWalkPlaceRootCoord(GfxCoord* rootCoord, const ActorTransform* placement, s16 yaw)
{
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->coord.t[0]   = placement->pos.vx;
    rootCoord->coord.t[1]   = placement->pos.vy;
    rootCoord->coord.t[2]   = placement->pos.vz;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
#endif

/// Places the receiver's model root at a position and yaw in its parent's space.
///
/// Handles `ACTOR_MESSAGE_PLACE` on a live TMD task; `SCRIPTED_WALK_WORK` must
/// select that receiver's live published work block. Borrows a non-NULL,
/// word-aligned placement through dispatch, reading only its XYZ position in
/// whole parent-coordinate units and signed Y angle in 4096 units per turn.
/// Stores the heading in `st.yaw` for later turns, replaces pitch, roll and
/// scale with a unit-scale yaw rotation, and invalidates the composition cache.
/// The root's parent and remaining walker state are preserved. Requires the
/// initialized scratch stack with 0x24 free bytes used by `gfxRotMatrixY`.
/// Returns 0; `messageId` and `unusedArgument` are ignored. Retains no pointer.
static s32 SCRIPTED_WALK_PLACE(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument)
{
    GfxCoord* rootCoord;
    s16       yaw;

    rootCoord                  = task->extra.tmd->coords;
    SCRIPTED_WALK_WORK->st.yaw = yaw = placement->rot.vy;
    _scriptedWalkPlaceRootCoord(rootCoord, placement, yaw);
    return 0;
}
