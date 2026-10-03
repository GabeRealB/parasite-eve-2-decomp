/* A nineteen-part scripted walker (Actor350500Work: the actor_motion 19-slot
 * head plus a variant byte and a buffer-free countdown) whose walk can run in
 * reverse. While the variant byte (field_4C4) is clear it faces away from its
 * target and backs toward it at -0.4 of its forward step; set, it walks
 * forward normally. Its per-frame update integrates a 16.16 step into the
 * root, ticks the rig, draws a ground shadow under part 1, relights from that
 * part and counts down to freeing its model buffers. The rest is its spawn-
 * walk message, spawn state and a four-mode visibility message. It builds on
 * actor_motion (actorMotionArrive19, actorMotionPlayAnim19,
 * gActorMotionAnimBanks19).
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_REVERSING_WALKER_H
#define SRC_SHARED_REVERSING_WALKER_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

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
