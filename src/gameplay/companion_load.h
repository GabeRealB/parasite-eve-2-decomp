#ifndef GAMEPLAY_PRIVATE_COMPANION_LOAD_H
#define GAMEPLAY_PRIVATE_COMPANION_LOAD_H

#include "types.h"

#include "gameplay/message.h"

#include "main/session_types.h"
#include "main/task_types.h"

s32 Gp_PickCompanion(void);

void Gp_ApplyNpcRoomSnd(void);

void Gp_SetupCompanionActor(GpActorArg* arg0, u16* arg1);

void Gp_MarkAreaVisited(GpAreaKey* arg0);

void Gp_SessionState1(Task* task);

void Gp_ResumeSessionTask(Task* task);

void Gp_LoadFinishTask(Task* task);

void Gp_LinkRoomObjectsSpawn(Task* task);

#endif // GAMEPLAY_PRIVATE_COMPANION_LOAD_H
