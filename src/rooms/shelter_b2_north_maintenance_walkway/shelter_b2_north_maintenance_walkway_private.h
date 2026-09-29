#ifndef SRC_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
#define SRC_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern SVECTOR D_shelter_b2_north_maintenance_walkway_80183C4C[2];

extern TaskDesc D_shelter_b2_north_maintenance_walkway_80183B48;

extern TaskDesc D_shelter_b2_north_maintenance_walkway_80183B54;

extern GpMsgEntry D_shelter_b2_north_maintenance_walkway_80183B60[6];

// Callbacks referenced by the overlay's shared data tables.
void func_shelter_b2_north_maintenance_walkway_8017D61C(Task*);

void func_shelter_b2_north_maintenance_walkway_8017D918(Task*);

s32 func_shelter_b2_north_maintenance_walkway_8017DA88(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_shelter_b2_north_maintenance_walkway_8017DC44(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_shelter_b2_north_maintenance_walkway_8017DC4C(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_shelter_b2_north_maintenance_walkway_8017DC54(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_shelter_b2_north_maintenance_walkway_8017DCE4(Task*, s32, s32, GpMessageArg);

#endif // SRC_ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
