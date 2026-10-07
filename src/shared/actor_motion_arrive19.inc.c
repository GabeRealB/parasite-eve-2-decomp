/* Part of the actor motion library; see actor_motion.h. */

#include "actor_motion_walk_helpers.h"

/// Stops the nineteen-part walk when neither horizontal axis gets closer.
///
/// Requires a live root coordinate and initialized `ActorMotion19WalkWork`.
/// Gaps use root-parent coordinate units, truncated to signed halfwords before
/// comparison; previous gaps store their absolute values. The movement step
/// seeds both previous gaps with `ACTOR_WALK_DISTANCE_NONE` before this step.
/// On arrival, requests the queued bank-0 clip with a five-frame blend through
/// `ACTOR_MOTION_PLAY19_HANDLER`, or `_actorMotionPlayAnim19` by default. The
/// clip must be loaded; restart behavior belongs to the selected handler.
/// Clears XYZ velocity and advances the walk step, retaining position and
/// fractional carry. Only X/Z previous gaps are updated while approaching.
static void _actorMotionArrive19(Task* task)
{
    ActorMotion19WalkWork* work;
    GfxCoord*              rootCoord;
    SVECTOR                distance;
    s32                    deltaX;
    s32                    deltaZ;
    AnimationPlayRequest   arrivalRequest;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // Full-word direction selects a low-halfword gap; narrowing is intentional.
    deltaX      = _actorMotionWalkAxisGap(&work->walk.target.vx, &rootCoord->coord.t[0]);
    distance.vx = deltaX;
    deltaZ      = _actorMotionWalkAxisGap(&work->walk.target.vz, &rootCoord->coord.t[2]);
    distance.vz = deltaZ;
    if (distance.vx >= work->walk.lastDistance.vx && distance.vz >= work->walk.lastDistance.vz) {
        arrivalRequest.source.index         = ACTOR_MOTION_WALK_ANIMATION_BANK;
        arrivalRequest.animationId          = work->model.nextAnimId;
        arrivalRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        arrivalRequest.blendFrames          = ACTOR_MOTION_WALK_BLEND_FRAMES;
        arrivalRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
#ifdef ACTOR_MOTION_PLAY19_HANDLER
        ACTOR_MOTION_PLAY19_HANDLER(task, ACTOR_MESSAGE_PLAY_ANIMATION, &arrivalRequest, 0);
#else
        _actorMotionPlayAnim19(task, ACTOR_MESSAGE_PLAY_ANIMATION, &arrivalRequest, 0);
#endif
        work->walk.velocity.vx = 0;
        work->walk.velocity.vy = 0;
        work->walk.velocity.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.lastDistance.vx = distance.vx < 0 ? -distance.vx : distance.vx;
    work->walk.lastDistance.vz = distance.vz < 0 ? -distance.vz : distance.vz;
}
