#ifndef GAMEFLOW_H
#define GAMEFLOW_H

#include "common.h"

#include "main/task.h"

// Functions — game-flow / fade (src/main/gameflow.c, 11E9C fade)

// --- APIs (from unknown_syms) ---
void GameFlow_StateByField34(Task* arg0);
void Fade_DrawOverlay(s32 r, s32 g, s32 b, s32 mode);
void GameFlow_InitSystems(void);
void GameFlow_SpawnMainWhenReady(Task* arg0);
void GameFlow_DispatchTable(Task* arg0);

#endif // GAMEFLOW_H
