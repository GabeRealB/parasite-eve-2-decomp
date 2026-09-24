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

void func_shelter_b4_lower_sewer_8017E6A0(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_lower_sewer_8017F038(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_lower_sewer_80180154(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b4_lower_sewer_80180580(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b4_lower_sewer_80181484(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_SHELTER_B4_LOWER_SEWER_H
