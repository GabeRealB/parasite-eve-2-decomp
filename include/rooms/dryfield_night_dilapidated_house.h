#ifndef ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_H
#define ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_H

#include "common.h"

#include "main/task.h"

#include "rooms/room_common.h"

/// The room task's three states: setup, idle, and exit.
extern const TaskFuncTable3 D_dryfield_night_dilapidated_house_8017D5DC;

/// The room's event gate.
s32 func_dryfield_night_dilapidated_house_8017D600(RoomEventReq* req, RoomEventMsg* msg);

#endif // ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_H
