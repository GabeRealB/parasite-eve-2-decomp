/* Code shared by one enemy species (actor_00400, which builds
 * actor_100400/200400, and actor_206100, a Neo Ark enemy) that attacks by
 * spawning a short-lived strike child. The child is a task holding a sphere
 * collision body that carries the attack, placed at the strike point. On spawn
 * and on hits it bursts impact sparks: a rotating six-frame spark billboard,
 * plus room-effect puffs and randomly aimed sparks depending on the burst
 * kind. The shared code also includes the species' world-space joint turn,
 * used to twist body parts by a third of a yaw each, and its step-along-
 * heading helper.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_STRIKER_ENEMY_H
#define SRC_SHARED_STRIKER_ENEMY_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

#include "gameplay/actor.h"

/// The head of a striker's beam or marker child work block, which each package
/// extends: the collision body the spawn state links and the teardown unlinks.
typedef struct StrikerChildWork {
    /* 0x00 */ byte               pad_0[0x8];
    /* 0x08 */ WorldCollisionBody obj;
} StrikerChildWork;

void strikerImpactBurst(GfxCoord* coord, u16 arg1, u16 arg2, u32 arg3);
void strikerDrawSpark(GfxCoord* arg0, u16 arg1, u16 arg2, s32 arg3);
void strikerTurnJoint(GfxCoord* coord, s16 yaw);
void strikerStepForward(Task* task, s16 arg1, s16 arg2);
void strikerStrikeTeardown(Task* task);

static inline s32       strikerAccumulateRotation(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2);
static inline GfxCoord* strikerLocalizeRotation(GfxCoord* arg0, MATRIX* arg1);

#endif /* SRC_SHARED_STRIKER_ENEMY_H */
