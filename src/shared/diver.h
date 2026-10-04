/* Code the Bog Diver (actor_00400, which builds actor_100400/200400) and the
 * Sea Diver (actor_206100) share. Both attack by
 * spawning a short-lived strike child. The child is a task holding a sphere
 * collision body that carries the attack, placed at the strike point. On spawn
 * and on hits it bursts impact sparks: a rotating six-frame spark billboard,
 * plus room-effect puffs and randomly aimed sparks depending on the burst
 * kind. The shared code also includes their world-space joint turn,
 * used to twist body parts by a third of a yaw each, and its step-along-
 * heading helper.
 *
 * Each package names its work block `DiverWork` before including the
 * fragments that reach it; the two layouts differ, but both carry the
 * animation request (`animRequest`, a `DIVER_ANIM_REQUEST_*`, with `animClip`,
 * `animStep`, `animBlend` and `animPlaying`) and the sub-state index
 * `subState` the shared states use.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_DIVER_H
#define SRC_SHARED_DIVER_H

#include "types.h"

#include "main/coord.h"
#include "main/task_types.h"

#include "gameplay/actor.h"

/// Values of `DiverWork::animRequest`, which each package's animation step
/// consumes once a frame.
enum {
    DIVER_ANIM_REQUEST_BLEND   = 1, // seek the slots to `animClip`, blending over `animBlend` frames
    DIVER_ANIM_REQUEST_RESET   = 2, // restart the slots on `animClip`
    DIVER_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// The head of a Diver's strike or marker child work block, which each package
/// extends: the collision body the spawn state links and the teardown unlinks.
typedef struct DiverChildWork {
    /* 0x00 */ byte               pad_0[0x8];
    /* 0x08 */ WorldCollisionBody obj;
} DiverChildWork;

void diverImpactBurst(GfxCoord* coord, u16 arg1, u16 arg2, u32 arg3);
void diverDrawSpark(GfxCoord* arg0, u16 arg1, u16 arg2, s32 arg3);
void diverTurnJoint(GfxCoord* coord, s16 yaw);
void diverStepForward(Task* task, s16 arg1, s16 arg2);
void diverStrikeTeardown(Task* task);
void diverRestartClip(Task* arg0);
void diverState7Enter(Task* arg0);

static inline s32       diverAccumulateRotation(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2);
static inline GfxCoord* diverLocalizeRotation(GfxCoord* arg0, MATRIX* arg1);

#endif /* SRC_SHARED_DIVER_H */
