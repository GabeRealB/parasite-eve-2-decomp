/* The Beta Generator (actor_105300) and the Proto Generator (actor_105400) of
 * the Neo Ark power plant rooms: a stationary body with a separate Life
 * Support system part task. While the Life Support system lives, the body
 * takes a tenth of the damage, cannot drop below 1 HP and regenerates a point
 * every five frames. Destroying the Life Support system toggles the power plant room
 * state and a per-variant game flag (0x147/0x148). The body's scale pulses
 * from clip tables and flickers on hits. On death it shrinks to an eighth
 * while spewing effects, turns semi-transparent, and waits for message bits to
 * release scripted state before being destroyed.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_GENERATOR_H
#define SRC_SHARED_GENERATOR_H

#include "types.h"

#include "main/task_types.h"

void generatorBodyHit(Task* arg0);
void generatorPulse(Task* arg0);
void generatorDeathState(Enemy* arg0, Task* arg1);
void generatorLifeSupportSpawn(Enemy* arg0, Task* arg1);
void generatorLifeSupportHit(Enemy* arg0, Task* arg1);
void generatorTickState(Enemy* arg0, Task* arg1);
void generatorRegenerate(Task* arg0);
void generatorTickPose(Task* arg0);
void generatorLifeSupportTeardown(Enemy* arg0, Task* arg1);
s32  generatorSetReleaseBits(Task* task, s32 msgId, ActorCommand* msg);

/* Defined by each package. */
void generatorUpdateColor(Task* arg0);

static inline void generatorTickPoseInline(Task* task);

#endif /* SRC_SHARED_GENERATOR_H */
