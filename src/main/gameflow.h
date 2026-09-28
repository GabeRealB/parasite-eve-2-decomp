#ifndef MAIN_PRIVATE_GAMEFLOW_H
#define MAIN_PRIVATE_GAMEFLOW_H

#include "types.h"

#include "main/task_types.h"

extern u16 D_8005ED8A;

void GameFlow_StateByField34(Task* task);

void GameFlow_DispatchTable5(Task* task);

void GameFlow_DispatchTable(Task* task);

/// Polls both controllers, negotiates analog mode and updates axes and vibration.
extern void Pad_PollControllers(void);

#endif // MAIN_PRIVATE_GAMEFLOW_H
