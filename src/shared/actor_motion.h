/* Actor motion: the play-animation message (0x7D3) and the scripted walk of
 * the actors a room script places and sends from point to point.
 *
 * The play handler serves any actor whose work block opens with a rig and a
 * model state, whether or not it walks. It rebinds the rig when the requested
 * bank changes, then blends or resets the slots into the requested clip and
 * ticks them. actorMotionPlayAnim drives a twenty-part rig,
 * actorMotionPlayAnim19 a nineteen-part one.
 *
 * The walk additionally needs the walk state directly after those two
 * members. 'Start walk' (0x7DD) latches a target position and rotation and
 * plays a start clip; the walk steps then face the target, walk until the
 * distance stops shrinking (then play the queued next clip), and turn 0x40 a
 * frame to the placement yaw before returning to idle. The step that sets
 * the actor moving is the package's own. Of the nineteen-part walk only the
 * arrival step, actorMotionArrive19, is here.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The package defines the animation bank tables the handlers index:
 * gActorMotionAnimBanks for the twenty-part handlers, gActorMotionAnimBanks19
 * for the nineteen-part ones. actorMotionArrive19 plays its next clip through
 * actorMotionPlayAnim19, which a package whose handler always restarts the
 * slots defines itself.
 */

#ifndef SRC_SHARED_ACTOR_MOTION_H
#define SRC_SHARED_ACTOR_MOTION_H

#include "types.h"

#include "actors/actor.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// What a twenty-part play request needs of the work block at `Task::work`:
/// the rig it binds and seeds, and the model state recording what the rig
/// plays.
///
/// It is a view of the front of a larger block, never an object of its own.
/// Every package that installs `actorMotionPlayAnim` opens its work block
/// with these two members, and the handler reaches nothing after them. What
/// follows is the package's own: a walker that runs the library's walk keeps
/// its walk state directly after (`ActorMotionWalkWork`), while an actor that
/// only plays clips keeps state of its own there.
///
/// Nothing but a play request binds `rig`, and one does so only when its bank
/// differs from `model.bank`. The package therefore allocates the block
/// zeroed and sets `model.bank` to `ACTOR_MODEL_STATE_NONE` before the first
/// request, which makes that request bind the rig whichever bank it names;
/// left at zero, a first request for bank 0 would seed and tick the slots of
/// an unbound context.
typedef struct {
    ActorAnimRig20  rig;   // Playback storage of the twenty-part model; the handler binds it to the requested bank and drives slots 1 to 19
    ActorModelState model; // What the rig plays; the handler keeps `bank`, `animId` and `ticking` and leaves the rest to the package
} ActorMotionPlayWork;
STATIC_ASSERT_SIZEOF(ActorMotionPlayWork, 0x4B8);

/// What the twenty-part walk needs of the work block at `Task::work`: the
/// head of a scripted walker, `ActorMotionPlayWork` with the walk state
/// directly after it.
///
/// `actorMotionStartWalk` and the steps `actorMotionFaceTarget`,
/// `actorMotionArrive` and `actorMotionTurnToYaw` run on it. The walk's clips
/// are played by `actorMotionPlayAnim`, which views the same block as
/// `ActorMotionPlayWork`, so the first two members are laid out as that
/// type's. What follows `walk` is the package's own.
typedef struct {
    ActorAnimRig20  rig;   // Playback storage of the twenty-part model
    ActorModelState model; // What the rig plays; `nextAnimId` is the clip the walk changes to on arrival and again as its closing turn ends
    ActorWalkState  walk;  // Destination, closing rotation, per-frame velocity and step of the walk in progress
} ActorMotionWalkWork;
STATIC_ASSERT_SIZEOF(ActorMotionWalkWork, 0x4FC);

/// What `actorMotionPlayAnim19` needs of the work block at `Task::work`: the
/// nineteen-part rig it binds and seeds, and the model state recording what
/// the rig plays.
///
/// Every package that installs the handler opens its work block with these
/// two members, and the handler reaches nothing after them. What follows is
/// the package's own: a walker that runs the library's arrival step keeps its
/// walk state directly after (`ActorMotion19WalkWork`), while other actors
/// keep state of their own there, or end the block a few bytes later.
typedef struct {
    ActorAnimRig19  rig;   // Playback storage of the nineteen-part model; the handler binds it to the requested bank and drives slots 1 to 18
    ActorModelState model; // What the rig plays; the handler keeps `bank`, `animId` and `ticking` and leaves the rest to the package
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
