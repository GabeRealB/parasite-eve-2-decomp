#ifndef SRC_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_DRYFIELD_NIGHT_PARKING_LOT_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_DRYFIELD_NIGHT_PARKING_LOT_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_night_parking_lot_8017EC54;

extern GpMsgEntry D_dryfield_night_parking_lot_8017EC60[6];

extern GpEvsCmd D_dryfield_night_parking_lot_8017ECB4[11];

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_night_parking_lot_8017D760(Task*);

s32 func_dryfield_night_parking_lot_8017D8D0(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_night_parking_lot_8017DAB4(Task*, s32, s32, GpMessageArg);

s32 func_dryfield_night_parking_lot_8017DB04(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_dryfield_night_parking_lot_8017DB0C(Task*, s32, s32, GpMessageArg);

s32 func_dryfield_night_parking_lot_8017DB34(Task*, s32, GpMsg13EF*, GpMessageArg);

void func_dryfield_night_parking_lot_8017DBA4(s32);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_PARKING_LOT_DRYFIELD_NIGHT_PARKING_LOT_PRIVATE_H
