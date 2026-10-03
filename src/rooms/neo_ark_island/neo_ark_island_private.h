#ifndef SRC_ROOMS_NEO_ARK_ISLAND_NEO_ARK_ISLAND_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_ISLAND_NEO_ARK_ISLAND_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

extern WorldCollisionGrid D_neo_ark_island_801826C8[1];

extern WorldCoordRoomLights D_neo_ark_island_80183CB0[1];

extern WorldCollisionTrigger D_neo_ark_island_80183CC8[4];

extern WorldCollisionTrigger D_neo_ark_island_80183DF8[3];

extern TaskMessageEntry D_neo_ark_island_80181B48[6];

extern TaskDesc D_neo_ark_island_80181B78;

// Callbacks referenced by the overlay's shared data tables.

void func_neo_ark_island_8017E844(Task*);

s32 func_neo_ark_island_8017E960(Task*, s32, s32, s32);

s32 func_neo_ark_island_8017E968(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_neo_ark_island_8017EA24(Task*, s32, s32, s32);

s32 func_neo_ark_island_8017EA2C(Task*, s32, s32, s32);

s32 func_neo_ark_island_8017EA34(Task*, s32, s32, s32);

#endif // SRC_ROOMS_NEO_ARK_ISLAND_NEO_ARK_ISLAND_PRIVATE_H
