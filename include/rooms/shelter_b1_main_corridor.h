#ifndef ROOMS_SHELTER_B1_MAIN_CORRIDOR_H
#define ROOMS_SHELTER_B1_MAIN_CORRIDOR_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

s32  func_shelter_b1_main_corridor_8017D620(RoomEventReq* req, RoomEventMsg* msg);
void func_shelter_b1_main_corridor_8017E070(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b1_main_corridor_8017E858(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b1_main_corridor_8017F488(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b1_main_corridor_8017FD60(GsCOORDINATE2* coord, s16 frame);
void func_shelter_b1_main_corridor_80180604(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b1_main_corridor_801817C8(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b1_main_corridor_801826CC(GsCOORDINATE2* coord, s16 frame, u8* rgb);

#endif // ROOMS_SHELTER_B1_MAIN_CORRIDOR_H
