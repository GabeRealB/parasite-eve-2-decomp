/* A nineteen-part scripted walker whose walk can run in reverse. While
 * ReverseWalkWork::walksForward is clear it faces away from its target and
 * backs toward it at -0.4 of its forward step; set, it walks forward
 * normally. Its per-frame update integrates a 16.16 step into the root, ticks
 * the rig, draws a ground shadow under part 1, relights from that part and
 * counts down to freeing its model buffers. The rest is its spawn-walk
 * message, spawn state and a four-mode visibility message. It builds on
 * actor_motion (_actorMotionArrive19, _actorMotionPlayAnim19,
 * gActorMotionAnimBanks19).
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_REVERSING_WALKER_H
#define SRC_SHARED_REVERSING_WALKER_H

#include "types.h"

#include "actors/actor.h"
#include "gameplay/message.h"

#include "main/task_types.h"

/// Work block of the reversing walker, kept at `Task::work`.
///
/// The spawn state allocates it zeroed at its full size. It opens as
/// `ActorMotion19WalkWork` does, which the actor-motion library's
/// nineteen-part play handler and arrival step run on; the two bytes after
/// that are the walker's own.
typedef struct {
    ActorAnimRig19  rig;           // Playback storage of the nineteen-part model; slots 1 to 18 are driven
    ActorModelState model;         // Light matrices lent to the model object, and what the rig plays
    ActorWalkState  walk;          // Destination, per-frame velocity and step of the walk in progress
    s8              walksForward;  // Which way a walk covers its distance, set by the package's command message (0 backing toward the target while facing away from it, 1 walking forward facing it)
    s8              freeCountdown; // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    byte            pad_4C6[0x2];
} ReverseWalkWork;
STATIC_ASSERT_SIZEOF(ReverseWalkWork, 0x4C8);

void reverseWalkUpdate(Task* arg0);
s32  reverseWalkStartMsg(Task* task, s32 arg1, ActorTransform* place, ActorMotionWalkAnim* anim);
void reverseWalkSpawn(Task* arg0);
void reverseWalkFaceTarget(Task* task);
void reverseWalkBeginMove(Task* arg0);
void reverseWalkTurnToYaw(Task* arg0);
s32  reverseWalkVisibilityMsg(Task* task, s32 arg1, s32 mode, s32 arg3);

/* Defined by each package. */
void reverseWalkIdle(Task* arg0);
void reverseWalkRunStep(Task* arg0);
void reverseWalkBindLighting(Task* arg0);
void reverseWalkExit(Task* arg0);

#endif /* SRC_SHARED_REVERSING_WALKER_H */
