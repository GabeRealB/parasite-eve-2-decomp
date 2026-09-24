#ifndef ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H
#define ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The event the room's gate `func_dryfield_night_water_tower_8017D60C`
/// latched: the incoming message and the request, kept for the event task it
/// spawns from `D_dryfield_night_water_tower_8017E6E0`, and the flag the gate
/// sets once it has done so.
extern RoomEventMsg D_dryfield_night_water_tower_80182C50;
extern RoomEventReq D_dryfield_night_water_tower_80182C5C;
extern u8           D_dryfield_night_water_tower_80182C58;
extern TaskDesc     D_dryfield_night_water_tower_8017E6E0;

/// The room's message table, which the entry task installs as its own
/// `Task::msgTable`.
extern GpMsgEntry D_dryfield_night_water_tower_8017E6EC[];

/// The room entry task's three states.
extern const TaskFuncTable3 D_dryfield_night_water_tower_8017D5DC;

s32  func_dryfield_night_water_tower_8017D60C(RoomEventReq* req, RoomEventMsg* msg);
void func_dryfield_night_water_tower_8017D770(Task* task);
s32  func_dryfield_night_water_tower_8017D8E0(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out);
s32  func_dryfield_night_water_tower_8017DA4C(s32 arg0, s32 arg1, s32 arg2);
void func_dryfield_night_water_tower_8017DADC(Task* task);
void func_dryfield_night_water_tower_8017DB28(Task* task);
void func_dryfield_night_water_tower_8017DC70(SVECTOR* arg0, s32 arg1);
void func_dryfield_night_water_tower_8017E458(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_DRYFIELD_NIGHT_WATER_TOWER_H
