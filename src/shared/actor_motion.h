/* Actor motion: the play-animation message (0x7D3) and the scripted walk of
 * the actors a room script places and sends from point to point.
 *
 * The play handler serves any actor whose work block opens with a rig and a
 * model state, whether or not it walks. It rebinds the rig when the requested
 * bank changes, then blends or resets the slots into the requested clip and
 * ticks them. _actorMotionPlayAnim drives a twenty-part rig,
 * _actorMotionPlayAnim19 a nineteen-part one.
 *
 * The walk additionally needs the walk state directly after those two
 * members. 'Start walk' (0x7DD) latches a target position and rotation and
 * plays a start clip; the walk steps then face the target, walk until the
 * distance stops shrinking (then play the queued next clip), and turn 0x40 a
 * frame to the placement yaw before returning to idle. The step that sets
 * the actor moving is the package's own. Of the nineteen-part walk only the
 * arrival step, _actorMotionArrive19, is here.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The package defines the animation bank tables the handlers index:
 * gActorMotionAnimBanks for the twenty-part handlers, gActorMotionAnimBanks19
 * for the nineteen-part ones. _actorMotionArrive19 plays its next clip through
 * the nineteen-part play handler. A package with different restart semantics
 * binds ACTOR_MOTION_PLAY19_HANDLER to its own handler before including this
 * header and the arrival fragment.
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
/// Every package that installs `_actorMotionPlayAnim` opens its work block
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
/// `_actorMotionStartWalk` and the steps `_actorMotionFaceTarget`,
/// `_actorMotionArrive` and `_actorMotionTurnToYaw` run on it. The walk's clips
/// are played by `_actorMotionPlayAnim`, which views the same block as
/// `ActorMotionPlayWork`, so the first two members are laid out as that
/// type's. What follows `walk` is the package's own.
typedef struct {
    ActorAnimRig20  rig;   // Playback storage of the twenty-part model
    ActorModelState model; // What the rig plays; `nextAnimId` is the clip the walk changes to on arrival and again as its closing turn ends
    ActorWalkState  walk;  // Destination, closing rotation, per-frame velocity and step of the walk in progress
} ActorMotionWalkWork;
STATIC_ASSERT_SIZEOF(ActorMotionWalkWork, 0x4FC);

/// What `_actorMotionPlayAnim19` needs of the work block at `Task::work`: the
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

/// What `_actorMotionArrive19` needs of the work block at `Task::work`: the
/// head of a nineteen-part scripted walker, `ActorMotion19PlayWork` with the
/// walk state directly after it.
///
/// The arrival step plays the walk's closing clip through
/// `_actorMotionPlayAnim19`, which views the same block as
/// `ActorMotion19PlayWork`, so the first two members are laid out as that
/// type's. What follows `walk` is the package's own.
typedef struct {
    ActorAnimRig19  rig;   // Playback storage of the nineteen-part model
    ActorModelState model; // What the rig plays; `nextAnimId` is the clip the walk changes to on arrival
    ActorWalkState  walk;  // Destination, per-frame velocity and step of the walk in progress
} ActorMotion19WalkWork;
STATIC_ASSERT_SIZEOF(ActorMotion19WalkWork, 0x4C4);

/// Bank and transition choices used by the scripted walk's generated requests.
enum {
    ACTOR_MOTION_WALK_ANIMATION_BANK = 0,
    ACTOR_MOTION_WALK_BLEND_FRAMES   = 5,
    ACTOR_MOTION_WALK_FIRST_STEP     = 0,
};

static void _actorMotionArrive(Task* task);
static void _actorMotionFaceTarget(Task* task);
static void _actorMotionTurnToYaw(Task* task);
static s32  _actorMotionPlayAnim(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _actorMotionStartWalk(Task* task, s32 messageId, const ActorTransform* placement, const ActorMotionWalkAnim* walkAnim);
static void _actorMotionArrive19(Task* task);
/// Selects a package's alternative nineteen-part play handler for arrival.
///
/// When defined before this header, the carrier must declare the function with
/// signature s32(Task*, s32, const AnimationPlayRequest*, s32). The arrival
/// fragment calls it once with a borrowed request. The binding is a function
/// identifier, never an evaluated expression; without it the shared handler is
/// declared and called. Keep it defined through inclusion of the arrival fragment.
#ifndef ACTOR_MOTION_PLAY19_HANDLER
/// Applies a changed animation clip to the carrier's nineteen-part model.
///
/// Requires a live TMD task whose work opens as `ActorMotion19PlayWork` does,
/// with `model.bank` initially `ACTOR_MODEL_STATE_NONE`. Request is borrowed
/// through the call and must not overlap playback state. Bank and clip must
/// index loaded entries of `gActorMotionAnimBanks19`; stored IDs narrow to
/// signed bytes before indexing. A bank change binds the rig and clears its
/// previous clip; an unchanged clip in the same bank leaves playback alone.
/// Slots 1..18 blend for `blendFrames` whole frames (normally 0..2047) when
/// blend is nonzero and already ticking, or reset otherwise, then tick once.
/// The rig borrows work-owned slots/poses, model coordinates and clip tables
/// until playback ends. Ignores message ID, collision choice and fourth argument.
/// Returns 0. Each carrier keeps a private instance of this implementation.
static s32 _actorMotionPlayAnim19(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
#endif

#endif /* SRC_SHARED_ACTOR_MOTION_H */
