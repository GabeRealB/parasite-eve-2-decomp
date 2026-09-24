#ifndef ROOMS_SHELTER_B2_OPERATING_ROOM_H
#define ROOMS_SHELTER_B2_OPERATING_ROOM_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

s32  func_shelter_b2_operating_room_8017D628(RoomEventReq* req, RoomEventMsg* msg);
void func_shelter_b2_operating_room_8017E118(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_operating_room_8017E95C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_operating_room_8017F478(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b2_operating_room_8017F6FC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b2_operating_room_8017FB20(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b2_operating_room_8018058C(GsCOORDINATE2* arg0, s32 arg1);

#endif // ROOMS_SHELTER_B2_OPERATING_ROOM_H
