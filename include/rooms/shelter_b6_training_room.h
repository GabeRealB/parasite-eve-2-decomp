#ifndef ROOMS_SHELTER_B6_TRAINING_ROOM_H
#define ROOMS_SHELTER_B6_TRAINING_ROOM_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// The three-state table `func_shelter_b6_training_room_8017D8E8` runs a task
/// through.
extern const TaskFuncTable3 D_shelter_b6_training_room_8017D5C4;

/// The task `func_shelter_b6_training_room_8017D974` drives, or NULL when
/// none is running.
extern Task* D_shelter_b6_training_room_80185C5C;

/// Draws a glowing capsule between the world points `arg0[0]` and `arg0[1]`.
void func_shelter_b6_training_room_8017E28C(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws a glowing disc at the world point `arg0`.
void func_shelter_b6_training_room_8017EAD0(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B6_TRAINING_ROOM_H
