/* Part of the paced walk library; see paced_walk.h. */

#ifndef SRC_SHARED_PACED_WALK_PLACE_ROOT
#define SRC_SHARED_PACED_WALK_PLACE_ROOT
/// Sets a model root's parent-space position and unit-scale yaw.
///
/// Borrows a writable, word-aligned root and readable position through this call.
/// XYZ are signed whole parent-coordinate units; the vector's fourth word is
/// ignored. `yaw` is signed, with 4096 units per turn. Replaces pitch, roll and
/// scale, preserves the parent link and stored Euler parameters, and marks the
/// composed matrix stale. Requires the rotation helper's initialized scratch
/// stack with room for 0x24 bytes. Retains neither pointer.
static inline void _pacedWalkPlaceRoot(GfxCoord* rootCoord, const VECTOR* position, s16 yaw)
{
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->coord.t[0]   = position->vx;
    rootCoord->coord.t[1]   = position->vy;
    rootCoord->coord.t[2]   = position->vz;
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
    _pacedWalkPlaceRoot(rootCoord, &placement->pos, yaw);
    return 0;
}
