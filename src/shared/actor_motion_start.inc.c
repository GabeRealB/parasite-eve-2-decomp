/* Part of the actor motion library; see actor_motion.h. */

#include "actor_motion_play_helpers.h"

/// Starts the carrier's twenty-part scripted walk toward a borrowed destination.
///
/// Handles `ACTOR_MESSAGE_WALK_TO` on a live TMD task with initialized
/// `ActorMotionWalkWork`. Copies XYZ position in the root parent's coordinate
/// frame and Euler angles in 4096 units per turn; the closing step uses yaw only.
/// The optional borrowed clips select bank-0 start and closing animations;
/// without them the defaults are 13 and 1. Both must exist in the carrier's
/// loaded bank and fit nonnegative signed bytes. Starts at the facing step,
/// then blends an already ticking rig for five whole frames or resets it.
/// Leaves velocity, previous gaps and fractional carry for the walk's later
/// steps. Neither payload is retained. Ignores message ID and returns 0.
static s32 _actorMotionStartWalk(Task* task, s32 messageId, const ActorTransform* placement, const ActorMotionWalkAnim* walkAnim)
{
    ActorMotionPlayWork* playWork;
    ActorMotionWalkWork* walkWork;
    AnimationPlayRequest startRequest;
    enum {
        ACTOR_MOTION_WALK_DEFAULT_START_CLIP   = 13,
        ACTOR_MOTION_WALK_DEFAULT_CLOSING_CLIP = 1,
    };

    walkWork                    = task->work;
    walkWork->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    walkWork->walk.motionStep   = ACTOR_MOTION_WALK_FIRST_STEP;
    walkWork->walk.target.vx    = placement->pos.vx;
    walkWork->walk.target.vy    = placement->pos.vy;
    walkWork->walk.target.vz    = placement->pos.vz;
    walkWork->walk.targetRot.vx = placement->rot.vx;
    walkWork->walk.targetRot.vy = placement->rot.vy;
    walkWork->walk.targetRot.vz = placement->rot.vz;
    startRequest.source.index   = ACTOR_MOTION_WALK_ANIMATION_BANK;
    if (walkAnim != NULL) {
        startRequest.animationId   = walkAnim->animationId;
        walkWork->model.nextAnimId = walkAnim->nextAnimId;
    } else {
        startRequest.animationId   = ACTOR_MOTION_WALK_DEFAULT_START_CLIP;
        walkWork->model.nextAnimId = ACTOR_MOTION_WALK_DEFAULT_CLOSING_CLIP;
    }
    startRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    startRequest.blendFrames          = ACTOR_MOTION_WALK_BLEND_FRAMES;
    startRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    playWork = task->work;
    _actorMotionApplyAnimationRequest(playWork, task->extra.tmd, &startRequest);
    return 0;
}
