#ifndef ROOMS_SHELTER_R48_H
#define ROOMS_SHELTER_R48_H

#include "common.h"

#include "main/task.h"

/// State handlers of the room task: its setup, an idle state, and `taskKill`.
extern const TaskFuncTable3 D_shelter_r48_8017D608;

void func_shelter_r48_8017E224(Task* task);

#endif // ROOMS_SHELTER_R48_H
