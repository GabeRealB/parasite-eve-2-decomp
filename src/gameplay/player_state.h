#ifndef GAMEPLAY_PRIVATE_PLAYER_STATE_H
#define GAMEPLAY_PRIVATE_PLAYER_STATE_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task.h"

s32 Gp_SetupAllyWeapon(void);

Task* Gp_SpawnAlly(GpActorArg* arg0, u16 arg1, s32 arg2, u16* arg3);

void func_80109FC4(Task* arg0);

void func_8010A42C(Task* arg0, s32 arg1);

void func_8010A670(Task* arg0);

s32 Gp_ApplyHpDamage(s16 arg0);

void func_8010AC54(Task* arg0);

void func_8010AD64(Task* arg0);

void Gp_PlayerStepSfx(Task* arg0);

void func_8010B210(Task* arg0);

void func_8010B3F8(Task* arg0);

void func_8010B520(Task* arg0);

#endif // GAMEPLAY_PRIVATE_PLAYER_STATE_H
