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

/// What `actorMotionPlayAnim19` needs of the work block at `Task::work`: the
/// nineteen-part rig it binds and seeds, and the model state recording what
/// the rig plays.
///
/// Every package that installs the handler opens its work block with these
/// two members. What follows is the package's own and the handler does not
/// reach it: a walker keeps its walk state there (`ActorMotion19WalkWork`),
/// while an actor that only plays clips keeps other state or ends the block
/// soon after.
typedef struct {
    ActorAnimRig19  rig;   // Playback storage of the nineteen-part model; the handler drives slots 1 to 18
    ActorModelState model; // Bank the rig is bound to and the clip its slots were last seeded with
} ActorMotion19PlayWork;
STATIC_ASSERT_SIZEOF(ActorMotion19PlayWork, 0x480);

/// What `actorMotionArrive19` needs of the work block at `Task::work`: the
/// head of a nineteen-part scripted walker, `ActorMotion19PlayWork` with the
/// walk state directly after it.
///
/// The arrival step plays the walk's closing clip through
/// `actorMotionPlayAnim19`, which views the same block as
/// `ActorMotion19PlayWork`, so the first two members are laid out as that
/// type's. What follows `walk` is the package's own.
typedef struct {
    ActorAnimRig19  rig;   // Playback storage of the nineteen-part model
    ActorModelState model; // What the rig plays; `nextAnimId` is the clip the walk changes to on arrival
    ActorWalkState  walk;  // Destination, per-frame velocity and step of the walk in progress
} ActorMotion19WalkWork;
STATIC_ASSERT_SIZEOF(ActorMotion19WalkWork, 0x4C4);

void actorMotionArrive(Task* arg0);
void actorMotionFaceTarget(Task* task);
void actorMotionTurnToYaw(Task* arg0);
s32  actorMotionPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);
s32  actorMotionStartWalk(Task* task, s32 arg1, ActorTransform* place, ActorMotionWalkAnim* anim);
void actorMotionArrive19(Task* arg0);
s32  actorMotionPlayAnim19(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);

#endif /* SRC_SHARED_ACTOR_MOTION_H */
