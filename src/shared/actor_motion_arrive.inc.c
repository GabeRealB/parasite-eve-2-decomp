/* Part of the actor motion library; see actor_motion.h. */

#include "actor_motion_walk_helpers.h"

/// Stops the twenty-part walk when neither horizontal axis gets closer.
///
/// Requires a live root coordinate and initialized `ActorMotionWalkWork`.
/// Gaps use root-parent coordinate units, truncated to signed halfwords before
/// comparison; previous gaps store their absolute values. The movement step
/// seeds both previous gaps with `ACTOR_WALK_DISTANCE_NONE` before this step.
/// On arrival, plays the queued bank-0 clip with a five-frame blend, clears XYZ
/// velocity and advances the walk step. The clip must be loaded. Neither
/// position nor fractional carry is changed; only X/Z previous gaps are updated.
static void _actorMotionArrive(Task* task)
{
    ActorMotionWalkWork* work;
    GfxCoord*            rootCoord;
    SVECTOR              distance;
    s32                  deltaX;
    s32                  deltaZ;
    AnimationPlayRequest arrivalRequest;

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
        _actorMotionPlayAnim(task, ACTOR_MESSAGE_PLAY_ANIMATION, &arrivalRequest, 0);
        work->walk.velocity.vx = 0;
        work->walk.velocity.vy = 0;
        work->walk.velocity.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.lastDistance.vx = distance.vx < 0 ? -distance.vx : distance.vx;
    work->walk.lastDistance.vz = distance.vz < 0 ? -distance.vz : distance.vz;
}
