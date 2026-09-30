/* A scripted 18-slot creature (0 HP, not lockable) that cutscenes show and
 * hide through message 2005. Its animation driver seeds its slots from a per-
 * transition start-frame table and cross-fades a second animation context into
 * slots 1-10. It eases a torso twist spread over joints 2-4 and a head turn on
 * joint 10, and plays the sound its per-package cue step returns.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_BLEND_RIG_CREATURE_H
#define SRC_SHARED_BLEND_RIG_CREATURE_H

#include "types.h"

#include "main/task_types.h"

void rigBlendTick(Task* task);
void rigAnimTick(Task* task);
void rigSpawn(Enemy* enemy, Task* task);
s32  rigSetVisibility(Task* task, s32 arg1, s32 arg2);

/* Defined by each package. */
s32  rigAnimCues(Task* task, Actor323000Work* work);
void rigExit(Task* task);

#endif /* SRC_SHARED_BLEND_RIG_CREATURE_H */
