/* Part of the paced walk library; see paced_walk.h. */

#ifndef SRC_SHARED_PACED_WALK_PLACE_ROOT
#define SRC_SHARED_PACED_WALK_PLACE_ROOT
/// Replaces a model root's rotation and translation in its existing parent's space.
///
/// Borrows a writable root and a readable placement for this call. `yaw` is
/// the signed heading already recorded in the receiver's work, in 1/4096 turns.
/// Replaces pitch, roll and scale with unit-scale yaw and invalidates composition.
static inline void _pacedWalkPlaceRoot(GfxCoord* rootCoord, const ActorTransform* placement, s16 yaw)
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
/// Handles `ACTOR_MESSAGE_PLACE` for a live TMD task whose `Task::work` is
/// the allocated type selected by `PACED_WALK_WORK_T`. The borrowed,
/// word-aligned placement supplies XYZ in whole parent-coordinate units and
/// signed yaw in 1/4096 turns; pitch, roll and the vectors' fourth components
/// are ignored. Records the heading in `st.yaw`, replaces the root rotation
/// at unit scale and invalidates composition. Consumes the placement during
/// dispatch without retaining it. Returns zero; `messageId` and
/// `unusedArgument` are ignored.
s32 PACED_WALK_PLACE(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument)
{
    GfxCoord*          rootCoord;
    PACED_WALK_WORK_T* work;
    s16                yaw;

    rootCoord    = task->extra.tmd->coords;
    work         = task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    _pacedWalkPlaceRoot(rootCoord, placement, yaw);
    return 0;
}
