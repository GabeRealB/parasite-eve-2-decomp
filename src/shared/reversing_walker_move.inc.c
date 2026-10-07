/* Part of the reversing walker library; see reversing_walker.h. */

/// Signed scale of the forward velocity while backing toward the destination.
#define REVERSE_WALK_BACKWARD_VELOCITY_SCALE (-0.4)

/// Starts movement along the root's forward or backward axis.
///
/// Walk step 1 requires initialized work and the root rotation set by the facing
/// step. The carrier's +Z velocity is 32 parent-coordinate units per tick in
/// signed 16.16; backing scales each component by -0.4 with truncation to s32.
/// Seeds the first arrival comparison and advances to step 2. Fractional carry
/// from an earlier walk is retained.
static void _reverseWalkBeginMove(Task* task)
{
    ReverseWalkWork* work;
    GfxCoord*        rootCoord;
    VECTOR           localVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    // Rotate displacement without including the root's translation.
    localVelocity = _gReverseWalkForward;
    if (work->walksForward == 0) {
        localVelocity.vx = localVelocity.vx * REVERSE_WALK_BACKWARD_VELOCITY_SCALE;
        localVelocity.vy = localVelocity.vy * REVERSE_WALK_BACKWARD_VELOCITY_SCALE;
        localVelocity.vz = localVelocity.vz * REVERSE_WALK_BACKWARD_VELOCITY_SCALE;
    }
    ApplyMatrixLV(&rootCoord->coord, &localVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walk.lastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walk.motionStep++;
}

#undef REVERSE_WALK_BACKWARD_VELOCITY_SCALE
