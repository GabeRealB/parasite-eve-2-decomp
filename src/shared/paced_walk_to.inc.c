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

/// Records a paced walker's heading and travel attempts toward a horizontal destination.
///
/// `ACTOR_MESSAGE_WALK_TO` callback for a live TMD task with writable
/// `PacedWalkWork` at `Task::work`. Borrows a word-aligned `target` during the
/// call, reading only its X/Z position in the model root's parent frame.
/// Replaces the root rotation with a unit-scale Y rotation and records the
/// signed-halfword heading in 4096 units per turn. Translation, parent and
/// composition stamp stay intact. Requires initialized scratch state with
/// room for the rotation helper's 0x24 bytes. No pointer is retained.
///
/// Stores the integer horizontal distance estimate divided by twelve
/// parent-coordinate units in signed-halfword `st.travel`, discarding the
/// remainder without clamping. Each signed X/Z offset and the sum of their
/// squares must fit signed 32-bit arithmetic. The animation update consumes
/// one attempt per twelve-unit step in TICK with requested walk clip 4,
/// including attempts suppressed by actor freezing. This command changes
/// neither animation selection nor request state; it does not start walking.
/// `messageId` and `unusedArgument` are ignored. Returns zero.
static s32 PACED_WALK_SET_WALK_TARGET(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument)
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
