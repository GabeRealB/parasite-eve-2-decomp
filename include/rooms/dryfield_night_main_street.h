#ifndef ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H
#define ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// The room's three-state jump table for the task `func_..._8017E0C0` drives.
extern const TaskFuncTable3 D_dryfield_night_main_street_8017D5F4;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_night_main_street_80188BC4;

s32  func_dryfield_night_main_street_8017D798(RoomEventReq* req, RoomEventMsg* msg);
void func_dryfield_night_main_street_8017E940(SVECTOR* arg0, s32 arg1);
void func_dryfield_night_main_street_8017F128(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_main_street_8017F608(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_dryfield_night_main_street_8017FD34(GsCOORDINATE2* arg0, u16 arg1, u16 arg2, u16 arg3);
void func_dryfield_night_main_street_8017FFF8(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_dryfield_night_main_street_8018041C(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_dryfield_night_main_street_80181220(GsCOORDINATE2* arg0, s32 arg1);
void func_dryfield_night_main_street_80181598(GsCOORDINATE2* arg0, s16 arg1, u8* rgb);

#endif
