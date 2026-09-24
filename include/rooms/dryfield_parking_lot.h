#ifndef ROOMS_DRYFIELD_PARKING_LOT_H
#define ROOMS_DRYFIELD_PARKING_LOT_H

#include "common.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_parking_lot_8017FB58;

/// The room's event gate.
s32 func_dryfield_parking_lot_8017D5E8(RoomEventReq* req, RoomEventMsg* msg);

/// The room entry task's states.
extern const TaskFuncTable3 D_dryfield_parking_lot_8017D5DC;

#endif // ROOMS_DRYFIELD_PARKING_LOT_H
