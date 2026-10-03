#ifndef SRC_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_DRYFIELD_NIGHT_PARKING_LOT_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_DRYFIELD_NIGHT_PARKING_LOT_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc gRoomEventTaskDesc;

extern TaskMessageEntry D_dryfield_night_parking_lot_8017EC60[6];

extern EvsCommand D_dryfield_night_parking_lot_8017ECB4[11];

// Callbacks referenced by the overlay's shared data tables.

s32 func_dryfield_night_parking_lot_8017DB04(Task*, s32, s32, s32);

s32 func_dryfield_night_parking_lot_8017DB0C(Task*, s32, s32, s32);

s32 func_dryfield_night_parking_lot_8017DB34(Task* task, s32 msgId, const void* firstArg, s32 arg3);

void func_dryfield_night_parking_lot_8017DBA4(s32);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_DRYFIELD_NIGHT_PARKING_LOT_PRIVATE_H
