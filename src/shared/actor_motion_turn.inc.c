/* Part of the actor motion library; see actor_motion.h. */

/// Turns the twenty-part walk's root to its destination yaw, then returns to idle.
///
/// Requires initialized `ActorMotionWalkWork` and a live model root coordinate.
/// Extracts the current Euler rotation and narrows the target yaw difference to
/// a signed halfword, without shortest-turn wrapping. Angles use 4096 units per
/// turn; steps by 64 each call until the gap is at most 64, then snaps to target.
/// Completion restarts the queued bank-0 clip with a five-frame blend and resets
/// the motion selectors. The clip must be loaded. Rebuilds the local rotation
/// and marks composition dirty, retaining translation and extracted pitch/roll;
/// the coordinate's stored Euler parameters are left unchanged.
static void _actorMotionTurnToYaw(Task* task)
{
    ActorMotionWalkWork* work;
    GfxCoord*            rootCoord;
    SVECTOR              rotation;
    AnimationPlayRequest closingRequest;
    s32                  yaw;
    s16                  yawGap;

    enum { ACTOR_MOTION_YAW_STEP = 64 };

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    gfxExtractSmallestEuler(&rotation, &rootCoord->coord);
    yawGap = (u16)work->walk.targetRot.vy - (u16)rotation.vy;
    if (ABS(yawGap) >= ACTOR_MOTION_YAW_STEP + 1) {
        yaw = rotation.vy;
        if (yawGap < 0) {
            rotation.vy = yaw - ACTOR_MOTION_YAW_STEP;
        } else {
            rotation.vy = yaw + ACTOR_MOTION_YAW_STEP;
        }
    } else {
        rotation.vy                         = work->walk.targetRot.vy;
        closingRequest.source.index         = ACTOR_MOTION_WALK_ANIMATION_BANK;
        closingRequest.animationId          = work->model.nextAnimId;
        closingRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        closingRequest.blendFrames          = ACTOR_MOTION_WALK_BLEND_FRAMES;
        closingRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actorMotionPlayAnim(task, ACTOR_MESSAGE_PLAY_ANIMATION, &closingRequest, 0);
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = ACTOR_MOTION_WALK_FIRST_STEP;
    }

    // Rebuild only the rotation; parent-space translation remains intact.
    gfxSetRotIdentity(&rootCoord->coord);
    RotMatrix(&rotation, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
