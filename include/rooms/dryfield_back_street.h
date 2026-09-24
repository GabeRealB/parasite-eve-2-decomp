#ifndef ROOMS_DRYFIELD_BACK_STREET_H
#define ROOMS_DRYFIELD_BACK_STREET_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// The room entry task's three states: set the room up, idle, end.
extern const TaskFuncTable3 D_dryfield_back_street_8017D5C4;

void func_dryfield_back_street_8017DC74(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_dryfield_back_street_8017E0A0(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_dryfield_back_street_8017EFA4(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_DRYFIELD_BACK_STREET_H
