#ifndef SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern GpMsgEntry D_neo_ark_garden_801813B0[5];

// Callbacks referenced by the overlay's shared data tables.

s32 func_neo_ark_garden_8017E840(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_neo_ark_garden_8017E848(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_neo_ark_garden_8017E8DC(Task*, s32, s32, TaskMessageArg);

s32 func_neo_ark_garden_8017E9AC(Task*, s32, TaskMessageArg, TaskMessageArg);

#endif // SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H
