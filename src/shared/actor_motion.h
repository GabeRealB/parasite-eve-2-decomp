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
 * position. The package defines the animation bank table the handlers index
 * as gActorMotionAnimBanks.
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

void actorMotionArrive(Task* arg0);
void actorMotionFaceTarget(Task* task);
void actorMotionTurnToYaw(Task* arg0);
s32  actorMotionPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);
s32  actorMotionStartWalk(Task* task, s32 arg1, ActorTransform* place, GpSpawnAnimArg* anim);

#endif /* SRC_SHARED_ACTOR_MOTION_H */
