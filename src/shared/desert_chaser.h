/* The scripted Desert Chaser of actor_323000 and actor_323400: an 18-slot rig
 * with 0 HP, not lockable, that cutscenes show and hide through message 2005. Its animation driver seeds its slots from a per-
 * transition start-frame table and cross-fades a second animation context into
 * slots 1-10. It eases a torso twist spread over joints 2-4 and a head turn on
 * joint 10, and plays the sound its per-package cue step returns.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_DESERT_CHASER_H
#define SRC_SHARED_DESERT_CHASER_H

#include "types.h"

#include "main/task_types.h"

void desertChaserBlendTick(Task* task);
void desertChaserAnimTick(Task* task);
void desertChaserSpawn(Enemy* enemy, Task* task);
s32  desertChaserSetVisibility(Task* task, s32 arg1, s32 arg2);

/* Defined by each package. */
s32 desertChaserAnimCues(Task* task, Actor323000Work* work);

void desertChaserFrameState(Enemy* enemy, Task* task);
void desertChaserPartEffect(Task* arg0, s16 part, s16 flags);
void desertChaserTask(Task* task);
void desertChaserHideState(Enemy* arg0, Task* arg1);
s32  desertChaserMsgPlayAnim(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);
void desertChaserExit(Task* task);

#endif /* SRC_SHARED_DESERT_CHASER_H */
