/* Part of the footstep walk library; see footstep_walk.h. */

/// Faces a planar target and schedules the whole moving updates needed to approach it.
///
/// Handles `ACTOR_MESSAGE_WALK_TO` for a live task with a
/// `FOOTSTEP_WALK_WORK_T` block and model root. `target` is borrowed xyz in
/// the root's parent space; Y is ignored. `mode` must be a
/// `FOOTSTEP_WALK_MODE_*` value (0 forward 60, 1 backward 15, 2 forward 25
/// parent-coordinate units per update). Backward mode faces away from the target.
/// The selected mode is stored as a signed halfword.
///
/// Stores floor(planar distance / positive step distance) in `st.travel`,
/// discarding a fractional update; movement begins only while a walk clip is
/// playing. Coordinate differences and their squared sum must fit signed 32
/// bits. Mode bounds are unchecked. Heading uses 4096 units per turn and is
/// narrowed to 16 bits. Returns zero and retains no target pointer.
static s32 _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode)
{
    GfxCoord*             rootCoord;
    FOOTSTEP_WALK_WORK_T* work;
    s32                   deltaX;
    s32                   deltaZ;
    s32                   stepDistance;
    s32                   planarDistance;
    s32                   targetYaw;

    rootCoord         = task->extra.tmd->coords;
    work              = task->work;
    gFootstepWalkMode = mode;
    deltaX            = target->vx - rootCoord->coord.t[0];
    deltaZ            = target->vz - rootCoord->coord.t[2];
    targetYaw         = ratan2(deltaX, deltaZ);
    work->st.yaw      = targetYaw;
    if (gFootstepWalkMode == FOOTSTEP_WALK_MODE_BACKWARD) {
        work->st.yaw = targetYaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    }
    gfxRotMatrixY(&rootCoord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
    // Travel counts updates, rather than distance units or animation records.
    planarDistance = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ);
    switch (gFootstepWalkMode) {
        case FOOTSTEP_WALK_MODE_FORWARD:
            stepDistance = FOOTSTEP_WALK_FORWARD_DISTANCE;
            break;
        case FOOTSTEP_WALK_MODE_BACKWARD:
            stepDistance = FOOTSTEP_WALK_BACKWARD_DISTANCE;
            break;
        case FOOTSTEP_WALK_MODE_SLOW_FORWARD:
            stepDistance = FOOTSTEP_WALK_SLOW_FORWARD_DISTANCE;
            break;
    }
    work->st.travel = planarDistance / stepDistance;
    return 0;
}
