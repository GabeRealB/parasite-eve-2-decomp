#ifndef DRYFIELD_G_R_KITCHEN_H
#define DRYFIELD_G_R_KITCHEN_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The room task's three-state table.
extern const TaskFuncTable3 D_dryfield_g_r_kitchen_8017D5DC;

/// The room's message table, installed on the room task by its entry state.
extern GpMsgEntry D_dryfield_g_r_kitchen_8017EBC0[];

s32  func_dryfield_g_r_kitchen_8017D5E8(RoomEventReq* req, RoomEventMsg* msg);
void func_dryfield_g_r_kitchen_8017D958(Task* task);
void func_dryfield_g_r_kitchen_8017D9FC(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

#endif // DRYFIELD_G_R_KITCHEN_H
