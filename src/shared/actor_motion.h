/* Actor motion: the play-animation message (0x7D3) and the walk sequence of
 * actors whose work block starts with a twenty-part rig, a model state and a
 * walk state. The play handler rebinds the rig when the requested bank
 * changes, then blends or resets the slots into the requested clip and ticks
 * them. 'Start walk' (0x7DD) latches a target position and rotation and plays
 * a start clip; the walk steps then face the target, walk until the distance
 * stops shrinking (then play the queued next clip), and turn 0x40 a frame to
 * the placement yaw before returning to idle.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The package defines the animation bank tables the handlers index
 * as gActorMotionAnimBanks and, for the nineteen-part walkers,
 * gActorMotionAnimBanks19. actorMotionArrive19 plays its next clip through
 * actorMotionPlayAnim19, which a package whose handler always restarts the
 * slots defines itself.
 */

#ifndef SRC_SHARED_ACTOR_MOTION_H
#define SRC_SHARED_ACTOR_MOTION_H

#include "types.h"

#include "actors/actor.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// The head of every work block these handlers run on. What follows is the
/// package's own.
typedef struct ActorMotionWork {
    ActorAnimRig20  rig;
    ActorModelState model;
    ActorWalkState  walk;
} ActorMotionWork;

/// The same head for the nineteen-part walkers, which the 19 variants run on.
typedef struct ActorMotion19Work {
    ActorAnimRig19  rig;
    ActorModelState model;
    ActorWalkState  walk;
} ActorMotion19Work;

void actorMotionArrive(Task* arg0);
void actorMotionFaceTarget(Task* task);
void actorMotionTurnToYaw(Task* arg0);
s32  actorMotionPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);
s32  actorMotionStartWalk(Task* task, s32 arg1, ActorTransform* place, GpSpawnAnimArg* anim);
void actorMotionArrive19(Task* arg0);
s32  actorMotionPlayAnim19(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);

#endif /* SRC_SHARED_ACTOR_MOTION_H */
