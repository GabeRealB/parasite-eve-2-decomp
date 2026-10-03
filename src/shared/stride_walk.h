/* A cutscene NPC walker that, when spawned with spawnArg1 set, carries a
 * second model hung off part 7 of its own (the pair-walk arrangement made
 * optional), keeps its work block in Task::work and walks fast: 30 units a
 * frame, distance counted in thirtieths. Each frame it relights itself from a
 * point 0x320 above its root, steps the animation, ramps a head-turn weight
 * toward the player and draws the walker shadow. Twelve scripted clips. Reuses
 * paced_walk's tick/reset/blend and walker's shadow.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_STRIDE_WALK_H
#define SRC_SHARED_STRIDE_WALK_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

void strideWalkSpawn(Enemy* enemy, Task* task);
void strideWalkUpdate(Task* task);
void strideWalkFrame(Enemy* enemy, Task* task);
s32  strideWalkPlay(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3);
s32  strideWalkSetVisibility(Task* task, s32 arg1, s32 flags, s32 arg3);
s32  strideWalkTo(Task* task, s32 arg1, ActorTransform* target, s32 arg3);
void strideWalkSubModelTask(Task* task);

/* Defined by each package. */
void strideWalkExit(Task* task);

#endif /* SRC_SHARED_STRIDE_WALK_H */
