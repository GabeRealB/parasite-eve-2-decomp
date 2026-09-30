/* The shared behaviour of one enemy species that ships as three packages
 * (actor_02300/05600/05700), which differ only in sound bank and some attack
 * states. It covers the idle and approach states with their player-proximity
 * check, the knocked-down and recoil states, the dead state that files the
 * enemy's pose, and three per-frame helpers: turning the root toward a target
 * yaw, easing out the hit tilt, and playing an animation's voice cues. Each
 * package defines its own voice-cue table and per-animation blend lengths
 * under the shared names.
 *
 * Include this header in the prologue and each fragment at its function's
 * position. The package defines its tables at its own positions under these
 * names:
 *
 *   s16 gLungerAnimBlendFrames[32]  blend-in length per animation, in frames
 *   s32 gLungerVoiceCues[17]        the voice-cue sound ids of its sound bank
 */

#ifndef SRC_SHARED_LUNGING_ENEMY_H
#define SRC_SHARED_LUNGING_ENEMY_H

#include "types.h"

#include "gameplay/enemy.h"

#include "main/task_types.h"

void lungerIdleState(Task* arg0);
void lungerApproachState(Task* arg0);
void lungerCheckProximity(Task* arg0);
void lungerRecoilState(Task* arg0);
void lungerCollapseState(Task* arg0);
void lungerDownedShiftState(Task* arg0);
void lungerDownedFinishState(Task* arg0);
void lungerTurnTowardTarget(Task* arg0);
void lungerDecayHitTilt(Task* arg0);
void lungerPlayAnimCues(Task* arg0);
void lungerDeadState(Enemy* arg0, Task* arg1);

void lungerHitTick(Task* arg0);
void lungerKnockdownState(Task* arg0);
void lungerLungeStrikeState(Task* arg0);
void lungerAimLaserSight(Task* arg0);
void lungerDrawLaserBeam(Task* arg0, SVECTOR* arg1, SVECTOR* arg2);
void lungerBulletSpawn(Enemy* arg0, Task* arg1);
void lungerBulletFly(Enemy* arg0, Task* arg1);
void lungerGunTick(Enemy* enemy, Task* task);
void lungerBulletDestroy(Enemy* arg0, Task* arg1);

void lungerSilenceScreamState(Task* arg0);
void lungerFrameState(Enemy* ctx, Task* actor);
void lungerBurstPartTick(Enemy* arg0, Task* arg1);

/* Defined by each package. */
void lungerTakeHits(Task* arg0);

static inline void lungerSpawnDust(Task* actor);
static inline void lungerApplyReaction(Task* actor);
static inline void lungerStepRoot(Task* actor);
static inline void lungerTickAnim(Task* actor);
static inline void lungerDraw(Task* actor, GfxCoord* coord);

#endif /* SRC_SHARED_LUNGING_ENEMY_H */
