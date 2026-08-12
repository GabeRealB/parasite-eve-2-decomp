#ifndef GAMEFLOW_H
#define GAMEFLOW_H

#include "common.h"

#include "main/task.h"

// Functions — game-flow (src/main/gameflow.c) and fade TILE (bootload.c)

void Fade_DrawOverlay(s32 r, s32 g, s32 b, s32 mode);
s32  Fade_StepIn(s32 arg0);
void Fade_StartWhite(void);
s32  Fade_StepOut(s32 arg0);

void GameFlow_StateByField34(Task* arg0);
void GameFlow_InitSystems(void);
void GameFlow_SpawnMenu(Task* arg0);
void GameFlow_WaitMenuDone(Task* arg0);
void GameFlow_CountdownAdvance(Task* arg0);
void GameFlow_SpawnMainWhenReady(Task* arg0);
void GameFlow_DispatchTable5(Task* arg0);
void GameFlow_CopySaveIds(Task* arg0);
void GameFlow_EnqueueDefaultLoad(Task* arg0);
void GameFlow_SpawnWhenIdle(Task* arg0);
void GameFlow_DispatchTable(Task* arg0);

#endif // GAMEFLOW_H
