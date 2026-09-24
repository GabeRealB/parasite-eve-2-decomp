#ifndef ROOMS_SHELTER_B4_LOWER_SEWER_H
#define ROOMS_SHELTER_B4_LOWER_SEWER_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// State handlers of the room task `func_shelter_b4_lower_sewer_8017D6D4`
/// runs, which copies the table to the stack and calls the entry for the
/// task's state: the room's setup, an idle state, and `taskKill`.
extern const TaskFuncTable3 D_shelter_b4_lower_sewer_8017D5C4;

/// Primitive cursor the water drawers allocate their quads and draw-mode
/// packets from; the water task's drawing state points it into the current
/// buffer's half of the primitive area each frame before drawing.
extern u8* D_shelter_b4_lower_sewer_80183E14;

void func_shelter_b4_lower_sewer_8017D72C(Task* task);
void func_shelter_b4_lower_sewer_8017DE8C(Task* task);
void func_shelter_b4_lower_sewer_8017E33C(Task* arg0);
void func_shelter_b4_lower_sewer_8017E37C(Task* task);

void func_shelter_b4_lower_sewer_8017E6A0(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_lower_sewer_8017F038(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_lower_sewer_8017F828(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b4_lower_sewer_8017FC14(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_lower_sewer_80180154(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b4_lower_sewer_80180580(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b4_lower_sewer_80181484(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_SHELTER_B4_LOWER_SEWER_H
