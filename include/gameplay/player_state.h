#ifndef GAMEPLAY_PLAYER_STATE_H
#define GAMEPLAY_PLAYER_STATE_H

#include "types.h"

#include "gameplay/actor.h"

#include "main/coord.h"
#include "main/session.h"
#include "main/task.h"

void Gp_TriggerPeState(s32 arg0, s32 arg1);

void Gp_BindActorD4(Task* arg0, SVECTOR3* arg1, s32 arg2);

void Gp_ResetActorMove(Task* arg0, s16 arg1);

void Gp_StopPlayerAnim(Task* arg0, s32 arg1);

void func_8010BF7C(Task* arg0, s32 arg1, s32 arg2);

void func_80109BB4(Task* arg0, GpRec18* arg1);

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

s32 func_8010BC70(GpCoord* arg0);

s32 func_8010BCF4(Task* arg0, VECTOR3* arg1);

void func_8010BD88(Task* arg0, VECTOR3* arg1);

void func_8010BE5C(Task* arg0, VECTOR3* arg1);

void func_8010C980(void* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3, s32 arg4, s32 arg5);

#endif // GAMEPLAY_PLAYER_STATE_H
