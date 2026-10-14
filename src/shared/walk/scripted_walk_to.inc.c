/* Part of the scripted walk library; see scripted_walk.h. */

/// Sets an approach heading and schedules whole movement updates toward a planar target.
///
/// Handles `ACTOR_MESSAGE_WALK_TO`; `messageId` is unused and the result is zero.
/// `task` must own a live TMD root and the allocation selected by
/// `SCRIPTED_WALK_WORK_T`. The non-NULL, word-aligned `target` is borrowed only
/// through dispatch: X/Z are whole units in the root parent's frame; Y is ignored.
/// Coordinate differences and their squared sum must fit signed 32 bits.
///
/// Stores the low signed halfword of `mode` in `SCRIPTED_WALK_MODE`. Modes
/// `SCRIPTED_WALK_MODE_*` select forward 60, backward 15 or forward 25 parent
/// units per moving update; backward faces away from the target. Other narrowed
/// modes use the 25-unit divisor but the update applies no translation.
/// Heading uses 4096 units per turn and narrows to the signed halfword `st.yaw`.
/// The root rotation is replaced at unit scale, preserving its translation.
/// Divides `SquareRoot0`'s integer distance approximation by the step distance
/// and stores the truncated quotient in the signed halfword `st.travel`.
/// Animation selection and composition invalidation belong to the caller/update.
/// An initialized scratch stack needs 0x24 aligned bytes for the rotation helper;
/// `SquareRoot0` overwrites the GTE leading-zero registers. No pointer is retained.
static s32 SCRIPTED_WALK_TO(Task* task, s32 messageId, const VECTOR* target, s32 mode)
{
    GfxCoord*             rootCoord;
    SCRIPTED_WALK_WORK_T* work;
    s32                   deltaX;
    s32                   deltaZ;
    s32                   stepDistance;
    s32                   planarDistance;
    s32                   targetYaw;

    /// Sets the approach yaw and replaces the model root's rotation at unit scale.
    ///
    /// Arguments must be side-effect-free pointers to this receiver's writable
    /// root and work block, and its signed target yaw in 4096 units per turn.
    /// The root argument is evaluated once; work and heading are evaluated
    /// repeatedly. Captures `SCRIPTED_WALK_MODE`, read after storing yaw;
    /// backward mode adds half a turn before narrowing, facing away from the target.
    /// The rotation helper preserves translation and borrows 0x24 aligned
    /// scratch bytes. Call as a standalone statement in a compound block;
    /// the macro is undefined after its sole call.
#define SCRIPTED_WALK_SET_APPROACH_HEADING(modelRoot, walkerWork, heading)                   \
    {                                                                                        \
        (walkerWork)->st.yaw = (heading);                                                    \
        if (SCRIPTED_WALK_MODE == SCRIPTED_WALK_MODE_BACKWARD) {                             \
            (walkerWork)->st.yaw = (heading) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;              \
        }                                                                                    \
        gfxRotMatrixY(&(modelRoot)->coord, (walkerWork)->st.yaw, GRAPHICS_ROTATION_REPLACE); \
    }

    rootCoord          = task->extra.tmd->coords;
    work               = task->work;
    SCRIPTED_WALK_MODE = mode;
    deltaX             = target->vx - rootCoord->coord.t[0];
    deltaZ             = target->vz - rootCoord->coord.t[2];

    // Backward travel keeps the model's front facing away from the destination.
    targetYaw = ratan2(deltaX, deltaZ);
    SCRIPTED_WALK_SET_APPROACH_HEADING(rootCoord, work, targetYaw);
#undef SCRIPTED_WALK_SET_APPROACH_HEADING

    // Count whole updates using the SDK's approximate planar distance.
    planarDistance = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ);
    stepDistance   = SCRIPTED_WALK_SHORT_FORWARD_DISTANCE;
    switch (SCRIPTED_WALK_MODE) {
        case SCRIPTED_WALK_MODE_FORWARD:
            stepDistance = SCRIPTED_WALK_FORWARD_DISTANCE;
            break;
        case SCRIPTED_WALK_MODE_BACKWARD:
            stepDistance = SCRIPTED_WALK_BACKWARD_DISTANCE;
            break;
        case SCRIPTED_WALK_MODE_FORWARD_SHORT:
            break;
    }
    work->st.travel = planarDistance / stepDistance;
    return 0;
}
