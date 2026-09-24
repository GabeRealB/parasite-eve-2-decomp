#ifndef ROOMS_DRYFIELD_NIGHT_PARKING_LOT_H
#define ROOMS_DRYFIELD_NIGHT_PARKING_LOT_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_night_parking_lot_8018156C;

/// The room entry task's three states, copied to the stack and indexed by
/// `Task::state`.
extern const TaskFuncTable3 D_dryfield_night_parking_lot_8017D5DC;

s32  func_dryfield_night_parking_lot_8017D5FC(RoomEventReq* req, RoomEventMsg* msg);
void func_dryfield_night_parking_lot_8017DE10(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_DRYFIELD_NIGHT_PARKING_LOT_H
