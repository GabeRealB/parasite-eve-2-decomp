/* Part of the reversing walker library; see reversing_walker.h. */

/// Initializes the reversing walker's owned work, messages and teardown callback.
///
/// Call once in task state 0 with a live nineteen-part TMD model, no existing
/// work and an owned enemy record in `spawnArg2.pointer`. Allocates zeroed
/// `ReverseWalkWork` on the primary heap, initially idle with backward walks
/// selected. No animation context is bound until a play request.
/// The model borrows the work's light and colour matrices until teardown.
/// Success advances the task to update state 1; failure releases the enemy
/// and starts task teardown, so the caller must not use them afterwards.
static void _reverseWalkSpawn(Task* task)
{
    enum { REVERSE_WALK_BUFFER_RELEASE_DISABLED = -1 };
    ReverseWalkWork* work;

    work = memCalloc(sizeof(ReverseWalkWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work               = work;
    work->model.animId       = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank         = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown      = REVERSE_WALK_BUFFER_RELEASE_DISABLED;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    _reverseWalkBindLighting(task);

    task->msgTable     = _gReverseWalkMessages;
    task->exitCallback = _reverseWalkExit;
    task->state       += 1;
}

/// Releases the walker's enemy record and begins task and model teardown.
///
/// Requires a live task with its owned primary-heap `Enemy` in
/// `spawnArg2.pointer`. Target tracking and actor locks are released before
/// children, work and model storage. Do not access the task or enemy afterwards.
static void _reverseWalkExit(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the walker's lighting matrices to its model for lighting and drawing.
///
/// Requires a live TMD task with allocated `ReverseWalkWork`. The model borrows
/// writable matrices in that work; keep it live until model use ends.
/// This binds storage without calculating or initializing either matrix.
static void _reverseWalkBindLighting(Task* task)
{
    TmdObject*       model;
    ReverseWalkWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Leaves walk state unchanged while no scripted walk is in progress.
///
/// The frame update still integrates velocity, ticks animation and draws.
/// The task parameter is unused but retains the `TaskFunc` callback signature.
static void _reverseWalkIdle(Task* task)
{
}

/// Runs the current phase of a scripted forward or backward walk.
///
/// Requires initialized `ReverseWalkWork` and `walk.motionStep` in 0..3:
/// face the target, begin movement, approach until arrival, then turn to the
/// destination yaw. Runs one phase per tick without checking the index.
static void _reverseWalkRunStep(Task* task)
{
    ReverseWalkWork*     work     = task->work;
    const TaskFuncTable4 handlers = _gReverseWalkStepHandlers;

    handlers.funcs[work->walk.motionStep](task);
}

/// Installs the walk heading as stored Euler angles and a pure root rotation.
///
/// Requires a live, writable root coordinate and separate readable rotation
/// XYZ, in 4096 units per turn. Builds Rx(X) * Ry(Y) * Rz(Z) in the root's
/// parent frame at unit scale, replacing the old rotation and scale.
/// Translation, parent and the stored rotation's fourth halfword stay intact;
/// the input's fourth halfword is unread. Composition is marked dirty so the
/// cached matrix is rebuilt before use. Neither input pointer is retained.
static inline void _reverseWalkApplyFacingRotation(GfxCoord* rootCoord, const SVECTOR* rotation)
{
    rootCoord->param.rot.vx = rotation->vx;
    rootCoord->param.rot.vy = rotation->vy;
    rootCoord->param.rot.vz = rotation->vz;
    RotMatrix(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Faces the root toward the destination, or almost half a turn away for backing.
///
/// Walk step 0 requires live work and a root coordinate with translation in the
/// destination's frame. Normalizes the XYZ offset and obtains yaw in 4096 units
/// per turn. Backing adds 2047 units, one unit short of half a turn.
/// Replaces pitch, roll and scale with a yaw rotation, invalidates composition
/// and advances to step 1.
static void _reverseWalkOrientForWalk(Task* task)
{
    ReverseWalkWork* work;
    GfxCoord*        rootCoord;
    VECTOR           targetOffset;
    SVECTOR          targetDirection;
    SVECTOR          facingRotation;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;

    targetOffset.vx = work->walk.target.vx - rootCoord->coord.t[0];
    targetOffset.vy = work->walk.target.vy - rootCoord->coord.t[1];
    targetOffset.vz = work->walk.target.vz - rootCoord->coord.t[2];
    VectorNormalS(&targetOffset, &targetDirection);

    facingRotation.vx = 0;
    facingRotation.vy = ratan2(targetDirection.vx, targetDirection.vz);
    facingRotation.vz = 0;
    if (work->walksForward == 0) {
        facingRotation.vy += ACTOR_TRANSFORM_ANGLE_HALF_TURN - 1;
    }

    _reverseWalkApplyFacingRotation(rootCoord, &facingRotation);
    work->walk.motionStep++;
}

/// Signed scale of the forward velocity while backing toward the destination.
///
/// Dimensionless double constant: each signed-16.16 component is multiplied
/// by -0.4 and converted back to s32 with truncation toward zero. Keep the
/// double precision; a float suffix or integer approximation changes rounding.
/// Applies only when walksForward is zero and is undefined after this fragment.
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
    localVelocity = _gReverseWalkForwardVelocity;
    if (work->walksForward == 0) {
        localVelocity.vx = localVelocity.vx * REVERSE_WALK_BACKWARD_VELOCITY_SCALE;
        localVelocity.vy = localVelocity.vy * REVERSE_WALK_BACKWARD_VELOCITY_SCALE;
        localVelocity.vz = localVelocity.vz * REVERSE_WALK_BACKWARD_VELOCITY_SCALE;
    }
    ApplyMatrixLV(&rootCoord->coord, &localVelocity, &work->walk.velocity);
    work->walk.lastDistance.vx = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vy = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.lastDistance.vz = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walk.motionStep++;
}

#undef REVERSE_WALK_BACKWARD_VELOCITY_SCALE

/// Finishes the walk by turning the root to the destination yaw and selecting idle.
///
/// Walk step 3 requires initialized work, a live root and loaded bank-0 clip 1.
/// Angles use 4096 units per turn. The yaw difference narrows to s16 without
/// wrapping at one turn; gaps above 96 units advance by 96, otherwise snap to
/// the target, request idle with a four-frame blend and clear the walk state.
/// Rebuilds the matrix from the extracted pitch/roll and updated yaw, retaining
/// translation and invalidating composition. Stored Euler parameters stay as
/// established by the initial facing step.
static void _reverseWalkTurnToYaw(Task* task)
{
    enum {
        REVERSE_WALK_TURN_STEP         = 0x60,
        REVERSE_WALK_TURN_BLEND_FRAMES = 4,
    };
    ReverseWalkWork*     work;
    GfxCoord*            rootCoord;
    SVECTOR              rotation;
    AnimationPlayRequest idleRequest;
    s32                  currentYaw;
    s16                  yawDifference;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    gfxExtractSmallestEuler(&rotation, &rootCoord->coord);
    yawDifference = (u16)work->walk.targetRot.vy - (u16)rotation.vy;
    if (ABS(yawDifference) >= REVERSE_WALK_TURN_STEP + 1) {
        currentYaw = rotation.vy;
        if (yawDifference < 0) {
            rotation.vy = currentYaw - REVERSE_WALK_TURN_STEP;
        } else {
            rotation.vy = currentYaw + REVERSE_WALK_TURN_STEP;
        }
    } else {
        rotation.vy                      = work->walk.targetRot.vy;
        idleRequest.source.index         = REVERSE_WALK_ANIMATION_BANK;
        idleRequest.animationId          = REVERSE_WALK_ANIMATION_IDLE;
        idleRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        idleRequest.blendFrames          = REVERSE_WALK_TURN_BLEND_FRAMES;
        idleRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &idleRequest, 0);
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = REVERSE_WALK_STEP_FACE_TARGET;
    }

    gfxSetRotIdentity(&rootCoord->coord);
    RotMatrix(&rotation, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
