#ifndef GAMEPLAY_PRIVATE_ENDING_H
#define GAMEPLAY_PRIVATE_ENDING_H

#include "main/task.h"

/// Colours and labels defined in `gameplay.c`, shared with `78.c`.
extern const TaskFuncTable6 Gp_PlayClockStates;

extern const char Gp_StrHP[];

extern const char Gp_StrMP[];

extern const char Gp_StrItem[];

void func_800A087C(Task* arg0);

void Gp_EndingTask(Task* arg0);

#endif // GAMEPLAY_PRIVATE_ENDING_H
