/* A stationary, pulsing enemy of the Neo Ark power plant rooms: a main body
 * with a separate weak-point part task. While the weak point lives, the body
 * takes a tenth of the damage, cannot drop below 1 HP and regenerates a point
 * every five frames. Destroying the weak point toggles the power plant room
 * state and a per-variant game flag (0x147/0x148). The body's scale pulses
 * from clip tables and flickers on hits. On death it shrinks to an eighth
 * while spewing effects, turns semi-transparent, and waits for message bits to
 * release scripted state before being destroyed.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_POWER_PLANT_POD_H
#define SRC_SHARED_POWER_PLANT_POD_H

#include "types.h"

#include "main/task_types.h"

void podBodyHit(Task* arg0);
void podPulse(Task* arg0);
void podDeathState(Enemy* arg0, Task* arg1);
void podWeakPointSpawn(Enemy* arg0, Task* arg1);
void podWeakPointHit(Enemy* arg0, Task* arg1);
void podTickState(Enemy* arg0, Task* arg1);
void podRegenerate(Task* arg0);
void podTickPose(Task* arg0);
void podWeakPointTeardown(Enemy* arg0, Task* arg1);
s32  podSetReleaseBits(Task* task, s32 msgId, ActorCommand* msg);

/* Defined by each package. */
void podUpdateColor(Task* arg0);

static inline void podTickPoseInline(Task* task);

#endif /* SRC_SHARED_POWER_PLANT_POD_H */
