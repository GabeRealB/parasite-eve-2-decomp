#ifndef ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H
#define ROOMS_DRYFIELD_NIGHT_MOTEL_BALCONY_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// The message and request the event gate latched for the event task.
extern RoomEventMsg D_dryfield_night_motel_balcony_8018F2D4;
extern RoomEventReq D_dryfield_night_motel_balcony_8018F2E0;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_night_motel_balcony_8018F2DC;

/// Descriptor of the event task the gate spawns.
extern TaskDesc D_dryfield_night_motel_balcony_801827F8;

/// The room task's three states: setup, the per-tick balcony event check,
/// and exit.
extern const TaskFuncTable3 D_dryfield_night_motel_balcony_8017D5DC;

s32  func_dryfield_night_motel_balcony_8017D694(RoomEventReq* req, RoomEventMsg* msg);
void func_dryfield_night_motel_balcony_8017EC58(SVECTOR* arg0, s32 arg1);
void func_dryfield_night_motel_balcony_8017F440(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif
