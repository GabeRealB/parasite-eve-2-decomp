#ifndef ROOMS_SHELTER_B2_BREEDING_ROOM_H
#define ROOMS_SHELTER_B2_BREEDING_ROOM_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// The room's three-entry task state table, dispatched by
/// `func_shelter_b2_breeding_room_8017D840` from a stack copy.
extern const TaskFuncTable3 D_shelter_b2_breeding_room_8017D5C4;

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`.
void func_shelter_b2_breeding_room_8017DB90(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws a glowing disc around the point `arg0`.
void func_shelter_b2_breeding_room_8017E3D4(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws a glowing ring around the coordinate's projected position.
void func_shelter_b2_breeding_room_8017F174(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// Draws a glowing disc at the coordinate's world position.
void func_shelter_b2_breeding_room_8017F598(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);

/// Draws a flat camera-facing quad around the coordinate's world position.
void func_shelter_b2_breeding_room_80180004(GsCOORDINATE2* arg0, s32 arg1);

#endif // ROOMS_SHELTER_B2_BREEDING_ROOM_H
