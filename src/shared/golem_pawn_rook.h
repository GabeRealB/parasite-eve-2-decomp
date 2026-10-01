/* The behaviour the Rook GOLEM (Beam Sword, actor_102300), the Pawn GOLEM
 * (Grenade Launcher, actor_105600) and the Rook GOLEM (Grenade Launcher,
 * actor_105700) share; the packages differ in sound bank and some attack
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
 *   s16 gGolemPawnRookAnimBlendFrames[32]  blend-in length per animation, in frames
 *   s32 gGolemPawnRookVoiceCues[17]        the voice-cue sound ids of its sound bank
 */

#ifndef SRC_SHARED_GOLEM_PAWN_ROOK_H
#define SRC_SHARED_GOLEM_PAWN_ROOK_H

#include "types.h"

#include "gameplay/enemy.h"

#include "main/task_types.h"

void golemPawnRookIdleState(Task* arg0);
void golemPawnRookApproachState(Task* arg0);
void golemPawnRookCheckProximity(Task* arg0);
void golemPawnRookRecoilState(Task* arg0);
void golemPawnRookCollapseState(Task* arg0);
void golemPawnRookDownedShiftState(Task* arg0);
void golemPawnRookDownedFinishState(Task* arg0);
void golemPawnRookTurnTowardTarget(Task* arg0);
void golemPawnRookDecayHitTilt(Task* arg0);
void golemPawnRookPlayAnimCues(Task* arg0);
void golemPawnRookDeadState(Enemy* arg0, Task* arg1);

void golemPawnRookHitTick(Task* arg0);
void golemPawnRookKnockdownState(Task* arg0);
void golemPawnRookLungeStrikeState(Task* arg0);
void golemPawnRookAimLaserSight(Task* arg0);
void golemPawnRookDrawLaserBeam(Task* arg0, SVECTOR* arg1, SVECTOR* arg2);
void golemPawnRookBulletSpawn(Enemy* arg0, Task* arg1);
void golemPawnRookBulletFly(Enemy* arg0, Task* arg1);
void golemPawnRookGunTick(Enemy* enemy, Task* task);
void golemPawnRookBulletDestroy(Enemy* arg0, Task* arg1);

void golemPawnRookSilenceScreamState(Task* arg0);
void golemPawnRookFrameState(Enemy* ctx, Task* actor);
void golemPawnRookBurstPartTick(Enemy* arg0, Task* arg1);

/* Defined by each package. */
void golemPawnRookTakeHits(Task* arg0);

static inline void golemPawnRookSpawnDust(Task* actor);
static inline void golemPawnRookApplyReaction(Task* actor);
static inline void golemPawnRookStepRoot(Task* actor);
static inline void golemPawnRookTickAnim(Task* actor);
static inline void golemPawnRookDraw(Task* actor, GfxCoord* coord);

#endif /* SRC_SHARED_GOLEM_PAWN_ROOK_H */
