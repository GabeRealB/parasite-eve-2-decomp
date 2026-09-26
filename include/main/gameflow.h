#ifndef GAMEFLOW_H
#define GAMEFLOW_H

#include "common.h"

#include "main/task.h"

// Functions — game-flow (src/main/gameflow.c) and fade TILE (bootload.c)

void Fade_DrawOverlay(s32 r, s32 g, s32 b, s32 mode);

void GameFlow_StateByField34(Task* arg0);
void GameFlow_DispatchTable5(Task* arg0);
void GameFlow_DispatchTable(Task* arg0);

#endif // GAMEFLOW_H
