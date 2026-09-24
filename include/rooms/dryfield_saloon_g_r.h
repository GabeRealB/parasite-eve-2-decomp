#ifndef ROOMS_DRYFIELD_SALOON_G_R_H
#define ROOMS_DRYFIELD_SALOON_G_R_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_saloon_g_r_8017DA18`: the entry tick
/// `func_dryfield_saloon_g_r_8017D9CC`, the idle state
/// `func_dryfield_saloon_g_r_8017DA10`, then `taskKill`.
extern const TaskFuncTable3 D_dryfield_saloon_g_r_8017D5DC;

/// The room's message table, installed on the room task by its entry tick.
extern GpMsgEntry D_dryfield_saloon_g_r_8017ECBC[];

s32  func_dryfield_saloon_g_r_8017D5E8(RoomEventReq* req, RoomEventMsg* msg);
void func_dryfield_saloon_g_r_8017DBB4(GsCOORDINATE2* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);
void func_dryfield_saloon_g_r_8017E430(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

#endif // ROOMS_DRYFIELD_SALOON_G_R_H
