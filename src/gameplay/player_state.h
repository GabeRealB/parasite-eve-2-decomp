#ifndef GAMEPLAY_PRIVATE_PLAYER_STATE_H
#define GAMEPLAY_PRIVATE_PLAYER_STATE_H

#include "types.h"

#include "actor.h"
#include "gameplay/actor_spawn_types.h"

#include "main/task_types.h"

Task* Gp_SetupAllyWeapon(void);

Task* Gp_SpawnAlly(const ActorSpawnTransform* spawnTransform, u16 arg1, s32 arg2, ActorSpawnOptions* options);

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
