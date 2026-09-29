#ifndef SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern GpMsgEntry D_neo_ark_garden_801813B0[5];

// Callbacks referenced by the overlay's shared data tables.
void func_neo_ark_garden_8017D64C(Task*);

void func_neo_ark_garden_8017E2A0(Task*);

s32 func_neo_ark_garden_8017E840(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_neo_ark_garden_8017E848(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_neo_ark_garden_8017E8DC(Task*, s32, s32, GpMessageArg);

s32 func_neo_ark_garden_8017E9AC(Task*, s32, GpMessageArg, GpMessageArg);

#endif // SRC_ROOMS_NEO_ARK_GARDEN_NEO_ARK_GARDEN_PRIVATE_H
