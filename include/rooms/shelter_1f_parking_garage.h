#ifndef ROOMS_SHELTER_1F_PARKING_GARAGE_H
#define ROOMS_SHELTER_1F_PARKING_GARAGE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// State table of the room's controller task `func_shelter_1f_parking_garage_8017DF14`.
extern const TaskFuncTable3 D_shelter_1f_parking_garage_8017D6A0;

void func_shelter_1f_parking_garage_8017E080(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_1f_parking_garage_8017E868(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_1f_parking_garage_8017EEB0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_1f_parking_garage_8017F2DC(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_1f_parking_garage_801801E0(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_SHELTER_1F_PARKING_GARAGE_H
