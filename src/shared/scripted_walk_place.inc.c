/* Part of the scripted walk library; see scripted_walk.h. */

#ifndef SRC_SHARED_SCRIPTED_WALK_PLACE_ROOT_COORD
#define SRC_SHARED_SCRIPTED_WALK_PLACE_ROOT_COORD
/// Places a scripted walker's model root at a parent-space position and yaw.
///
/// Borrows a live, writable, word-aligned root and a readable, word-aligned
/// position through the call; neither pointer is retained. XYZ uses signed
/// whole units in the root's existing parent's space; the vector's fourth
/// word is ignored. `yaw` is signed, in 4096 units per turn, without a
/// normalization requirement. Replaces pitch, roll and scale with unit-scale
/// yaw and marks composition dirty. Preserves the parent and stored Euler
/// state; leaves the cached matrix stale for the next composition. Requires
/// an initialized scratch stack with 0x24 free bytes, released before returning.
static __inline__ void _scriptedWalkPlaceRootCoord(GfxCoord* rootCoord, const VECTOR* parentPosition, s16 yaw)
{
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->coord.t[0]   = parentPosition->vx;
    rootCoord->coord.t[1]   = parentPosition->vy;
    rootCoord->coord.t[2]   = parentPosition->vz;
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
    _scriptedWalkPlaceRootCoord(rootCoord, &placement->pos, yaw);
    return 0;
}
