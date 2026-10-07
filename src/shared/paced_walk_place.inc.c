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
/// Handles `ACTOR_MESSAGE_PLACE` for a live TMD task with at least one writable
/// coordinate and the allocated `PACED_WALK_WORK_T` at `Task::work`. Borrows
/// a non-NULL, readable, word-aligned placement through dispatch. Reads XYZ in
/// whole units of the root's existing parent's space and signed yaw in 4096
/// units per turn, without normalization; other placement components are unused.
/// Records the heading in `st.yaw` and installs a unit-scale yaw rotation and
/// XYZ translation, marking the composed matrix stale. The root's parent link
/// and stored Euler parameters stay intact. Requires the initialized scratch
/// stack with 0x24 free bytes used by the rotation helper, released before
/// returning. Returns zero; `messageId` and `unusedArgument` are ignored.
/// Retains neither the placement nor any pointer into it.
static s32 PACED_WALK_PLACE(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument)
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
