/* Part of the paced walk library; see paced_walk.h. */

#ifndef SRC_SHARED_PACED_WALK_FACE_TRAVEL_TARGET
#define SRC_SHARED_PACED_WALK_FACE_TRAVEL_TARGET
/// Faces a paced walker's local Z axis along a displacement in its root parent's X/Z plane.
///
/// `deltaX` and `deltaZ` are signed parent-coordinate units, not an absolute
/// destination. The SDK bearing is narrowed to a signed halfword before both
/// recording `st.yaw` and installing the rotation, with 4096 units per turn.
/// A zero displacement still goes through the SDK bearing calculation.
///
/// Replaces pitch, roll and scale with a pure Y rotation at unit scale,
/// preserving translation, parent and composition stamp. Borrows live,
/// writable `rootCoord` and `work` without retaining either pointer. Requires
/// an initialized scratch stack with room for the rotation helper's 0x24 bytes.
static inline void _pacedWalkFaceTravelTarget(GfxCoord* rootCoord, PacedWalkWork* work, s32 deltaX, s32 deltaZ)
{
    s16 yaw;

    yaw          = ratan2(deltaX, deltaZ);
    work->st.yaw = yaw;
    gfxRotMatrixY(&rootCoord->coord, yaw, GRAPHICS_ROTATION_REPLACE);
}
#endif

/// Sets a paced walker's heading and remaining travel attempts from a destination.
///
/// Handles `ACTOR_MESSAGE_WALK_TO` for a live TMD task with writable
/// `PacedWalkWork`. Borrows word-aligned target X/Z in the root parent's
/// coordinate frame; height and orientation are ignored, and no pointer is
/// retained. Replaces the root rotation at unit scale and records its signed
/// yaw in 4096 units per turn, preserving translation and the composition stamp.
/// Rotation requires initialized scratch state.
///
/// Divides the integer horizontal distance estimate by twelve parent-coordinate
/// units and stores the low signed halfword in `st.travel`, discarding the
/// remainder without clamping. Offsets and their squared sum must fit the
/// nonnegative signed 32-bit square-root input. Later walking updates consume
/// one attempt per twelve-unit step, including frozen attempts. This command
/// leaves animation selection and request state intact; walking requires clip 4.
/// `messageId` and `unusedArgument` are ignored. Returns zero.
s32 PACED_WALK_SET_WALK_TARGET(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument)
{
    enum {
        PACED_WALK_STEP_UNITS = 12,
    };

    GfxCoord*      rootCoord;
    PacedWalkWork* work;
    s32            deltaX;
    s32            deltaZ;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    deltaX    = target->pos.vx - rootCoord->coord.t[0];
    deltaZ    = target->pos.vz - rootCoord->coord.t[2];
    _pacedWalkFaceTravelTarget(rootCoord, work, deltaX, deltaZ);
    // Schedule whole travel attempts; this does not start or reseed a clip.
    work->st.travel = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ) / PACED_WALK_STEP_UNITS;
    return 0;
}
