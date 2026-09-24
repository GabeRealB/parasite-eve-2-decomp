#ifndef ROOMS_DRYFIELD_GENERAL_STORE_H
#define ROOMS_DRYFIELD_GENERAL_STORE_H

#include "common.h"

#include "main/task.h"

#include "rooms/room_common.h"

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_general_store_8017DF5C`.
extern const TaskFuncTable3 D_dryfield_general_store_8017D5F4;

s32 func_dryfield_general_store_8017D600(RoomEventReq* req, RoomEventMsg* msg);

#endif // ROOMS_DRYFIELD_GENERAL_STORE_H
