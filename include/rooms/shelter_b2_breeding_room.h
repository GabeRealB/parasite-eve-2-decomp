#ifndef ROOMS_SHELTER_B2_BREEDING_ROOM_H
#define ROOMS_SHELTER_B2_BREEDING_ROOM_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// The room's three-entry task state table, dispatched by
/// `func_shelter_b2_breeding_room_8017D840` from a stack copy.
extern const TaskFuncTable3 D_shelter_b2_breeding_room_8017D5C4;

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`.
void func_shelter_b2_breeding_room_8017DB90(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws a glowing disc around the point `arg0`.
void func_shelter_b2_breeding_room_8017E3D4(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B2_BREEDING_ROOM_H
