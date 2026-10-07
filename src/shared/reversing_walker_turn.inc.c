/* Part of the reversing walker library; see reversing_walker.h. */

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
