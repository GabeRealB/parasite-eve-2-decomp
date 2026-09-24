#ifndef ROOMS_SHELTER_B3_ELEVATOR_HALL_H
#define ROOMS_SHELTER_B3_ELEVATOR_HALL_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

s32  func_shelter_b3_elevator_hall_8017D62C(RoomEventReq* req, RoomEventMsg* msg);
void func_shelter_b3_elevator_hall_8017DFB0(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b3_elevator_hall_8017F1A8(GsCOORDINATE2* arg0, s32 arg1, u8* arg2);
void func_shelter_b3_elevator_hall_80180324(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_SHELTER_B3_ELEVATOR_HALL_H
