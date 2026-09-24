#ifndef ROOMS_DRYFIELD_NIGHT_BACK_STREET_H
#define ROOMS_DRYFIELD_NIGHT_BACK_STREET_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// The room entry task's three states: set the room up, idle, end.
extern const TaskFuncTable3 D_dryfield_night_back_street_8017D5C4;

void func_dryfield_night_back_street_8017D920(SVECTOR* arg0, s32 arg1);
void func_dryfield_night_back_street_8017E108(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_back_street_8017E634(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_dryfield_night_back_street_8017EA60(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_dryfield_night_back_street_8017F2E4(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_dryfield_night_back_street_8017F964(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_DRYFIELD_NIGHT_BACK_STREET_H
