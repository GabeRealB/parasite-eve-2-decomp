#ifndef ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H
#define ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H

#include "common.h"
#include <psyq/libgte.h>

#include "main/task.h"

/// State handlers of the room task: its setup, an idle state, and `taskKill`.
extern const TaskFuncTable3 D_shelter_b1_golem_freezer_1_8017D5C4;

void func_shelter_b1_golem_freezer_1_8017D6EC(Task* task);
void func_shelter_b1_golem_freezer_1_8017DC5C(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H
