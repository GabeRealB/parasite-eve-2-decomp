#ifndef ROOMS_DRYFIELD_MAIN_STREET_H
#define ROOMS_DRYFIELD_MAIN_STREET_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// The room entry task's three states: set the room up, idle, end.
extern const TaskFuncTable3 D_dryfield_main_street_8017D5F4;

/// The task the room's spawn helper starts; cleared or killed by the room's
/// other entry points.
extern Task* D_dryfield_main_street_80185630;

s32  func_dryfield_main_street_8017D798(RoomEventReq* req, RoomEventMsg* msg);
void func_dryfield_main_street_8017F18C(GsCOORDINATE2* coord, s32 arg1, s32 arg2, u8* rgb);
void func_dryfield_main_street_8017F5B8(GsCOORDINATE2* coord, s32 arg1, u8* rgb);
void func_dryfield_main_street_801804BC(GsCOORDINATE2* coord, s16 arg1, u8* rgb);

#endif
