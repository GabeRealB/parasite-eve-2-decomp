#ifndef GAMEPLAY_HUD_SPRITES_H
#define GAMEPLAY_HUD_SPRITES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/view.h"

#include "main/session_types.h"
#include "main/task_types.h"

s32 Gp_IsDebugAttachRoom(void);

void Gp_TriggerPeIfArmed(void);

void Gp_WorldToLocal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2);

s32 Gp_TrySpawnViewTask(GpViewRec* arg0);

void Gp_ApplyView(GpViewRec* arg0);

void Gp_SpawnViewTasks(void);

GpViewRec* Gp_GetStageView(GameLocationKey* arg0);

void Gp_SpawnCurView(s32 arg0);

s32 Gp_SpendMp(s32 arg0);

void Gp_ApplyViewTask(Task* task);

void Gp_ViewGateTask(Task* task);

void func_800A77B4(Task* arg0);

void func_800A8654(Task* task);

#endif // GAMEPLAY_HUD_SPRITES_H
