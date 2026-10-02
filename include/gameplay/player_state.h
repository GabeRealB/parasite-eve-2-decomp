#ifndef GAMEPLAY_PLAYER_STATE_H
#define GAMEPLAY_PLAYER_STATE_H

#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/message.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

void Gp_TriggerPeState(s32 arg0, s32 arg1);

void Gp_BindActorD4(Task* arg0, SVECTOR3* arg1, s32 arg2);

void Gp_ResetActorMove(Task* arg0, s16 arg1);

void Gp_StopPlayerAnim(Task* arg0, s32 arg1);

void func_8010BF7C(Task* arg0, s32 arg1, s32 arg2);

void func_80109BB4(Task* arg0, WorldCollisionContact* arg1);

void func_8010BFCC(Task* arg0);

void func_8010B9A4(Task* arg0);

void Gp_TrackAllyLockTarget(Task* arg0, s32 arg1);

void Gp_EndPlayerActorTask(Task* arg0);

void func_8010A9D0(Task* arg0);

s32 Gp_HurtAlly(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_8010B2A0(s32 arg0, s32 arg1);

s32 func_8010C058(void);

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
s32 func_8010C4F0(Task* task, s32 msgId, AnimationPlayRequest* request);
s32 func_8010C688(Task*, s32, ActorTransform* transform, s32);
s32 func_8010C30C(Task*);
s32 func_8010C6C8(Task*, s32, ActorTransform* transform, GameActorMoveAnim* moveAnim);
/// Installs a borrowed companion animation-set table while preserving player state.
s32  func_8010C648(Task* task, s32 msgId, AnimationPlayRequest* request);
s32  Gp_CopyAllyAnim(Task*, s32, const AnimationBankCopyRequest* request);
s32  func_8010C75C(Task*, s32, GameActorButtonPressHold*);
void Gp_MoveActorByKeep(Task*, s32, GpMoveArg*);
s32  func_8010C708(Task*, s32, ActorTransform* transform, GameActorMoveAnim* moveAnim);

#endif // GAMEPLAY_PLAYER_STATE_H
