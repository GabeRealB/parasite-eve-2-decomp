#ifndef GAMEPLAY_PLAYER_STATE_H
#define GAMEPLAY_PLAYER_STATE_H

#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

/// Subtracts `amount` MP from the player, draining the remainder when it is insufficient.
///
/// `amount` is a nonnegative count of Parasite Energy points. Returns 1 when
/// all points were available (including a zero request), otherwise sets MP
/// to zero and returns 0. Used for hostile drains as well as spending;
/// changes neither maximum MP nor HP and retains no pointers.
s32 playerStateSpendMp(s32 amount);

void Gp_TriggerPeState(s32 arg0, s32 arg1);

void Gp_BindActorD4(Task* arg0, SVECTOR3* arg1, s32 arg2);

void Gp_ResetActorMove(Task* arg0, s16 arg1);

void Gp_StopPlayerAnim(Task* arg0, s32 arg1);

/// Sets the companion's delay before its next idle decision, in active behavior ticks.
///
/// `task->work` must hold a live `GameActor` with a non-NULL `companionWork`.
/// Stores `baseTicks + (rand() & randomMask)` in the signed 16-bit timer.
/// Callers must keep that sum in 0..32767; the mask selects random bits.
/// Updates that do not run the companion's normal behavior do not advance
/// the timer.
void companionSetDecisionDelay(Task* task, s32 baseTicks, s32 randomMask);

void func_80109BB4(Task* arg0, WorldCollisionContact* arg1);

void func_8010BFCC(Task* arg0);

void func_8010B9A4(Task* arg0);

void Gp_TrackAllyLockTarget(Task* arg0, s32 arg1);

void Gp_EndPlayerActorTask(Task* arg0);

void func_8010A9D0(Task* arg0);

s32 Gp_HurtAlly(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_8010B2A0(s32 arg0, s32 arg1);

/// Indices selecting companion behavior distributions by remaining HP.
enum {
    COMPANION_HEALTH_BAND_ABOVE_HALF      = 0,
    COMPANION_HEALTH_BAND_ABOVE_QUARTER   = 1,
    COMPANION_HEALTH_BAND_AT_MOST_QUARTER = 2
};

/// Returns the live save's companion HP band (0 above half, 1 above a quarter, 2 otherwise).
///
/// Half and quarter thresholds use signed right shifts of maximum HP. Band 1
/// covers HP above the quarter threshold through the half threshold; band 2
/// includes equality at the quarter threshold. Zero or negative HP also belongs
/// to band 2 for a nonnegative maximum.
s32 companionGetHealthBand(void);

void func_8010C180(Task* arg0);

void func_8010B610(Task* arg0);

void func_8010ABD4(Task* arg0);

s32 func_8010BC70(GfxCoord* arg0);

s32 func_8010BCF4(Task* arg0, VECTOR3* arg1);

void func_8010BD88(Task* arg0, VECTOR3* arg1);

/// Turns the actor's aim yaw toward a point in game coordinates beneath the view node.
///
/// Uses model part 4 as the origin. Each call changes the yaw offset by at most
/// 0x20 angle units and keeps its magnitude strictly below 0x1A0 (0x1000 per turn).
void func_8010BE5C(Task* task, VECTOR3* targetPoint);

void func_8010C980(void* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3, s32 arg4, s32 arg5);

// Message handlers addressed by the companion overlays' dispatch tables.
/// Plays an indexed companion animation and applies its world collision choice.
s32 func_8010C4F0(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg);
s32 func_8010C688(Task* task, s32 msgId, ActorTransform* transform, s32);
s32 func_8010C30C(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);
s32 func_8010C6C8(Task* task, s32 msgId, ActorTransform* transform, GameActorMoveAnim* moveAnim);
/// Installs a borrowed companion animation-set table while preserving player state.
s32 func_8010C648(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg);
s32 Gp_CopyAllyAnim(Task* task, s32 msgId, const AnimationBankCopyRequest* request, s32 unusedSecondArg);
s32 func_8010C75C(Task* task, s32 msgId, GameActorButtonPressHold*, s32 unusedSecondArg);
s32 Gp_MoveActorByKeep(Task* task, s32 msgId, GameActorMoveBy*, s32 unusedSecondArg);
s32 func_8010C708(Task* task, s32 msgId, ActorTransform* transform, GameActorMoveAnim* moveAnim);

#endif // GAMEPLAY_PLAYER_STATE_H
