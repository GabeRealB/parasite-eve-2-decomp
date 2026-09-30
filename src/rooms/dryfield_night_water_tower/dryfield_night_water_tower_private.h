#ifndef SRC_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

extern TaskDesc D_dryfield_night_water_tower_8017E6E0;

extern GpMsgEntry D_dryfield_night_water_tower_8017E6EC[6];

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_night_water_tower_8017D770(Task*);

s32 func_dryfield_night_water_tower_8017D8E0(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_night_water_tower_8017DA4C(Task*, s32, s32, TaskMessageArg);

s32 func_dryfield_night_water_tower_8017DA9C(Task*, s32, TaskMessageArg, TaskMessageArg);

s32 func_dryfield_night_water_tower_8017DAA4(Task*, s32, s32, TaskMessageArg);

s32 func_dryfield_night_water_tower_8017DAD4(Task*, s32, TaskMessageArg, TaskMessageArg);

#endif // SRC_ROOMS_DRYFIELD_NIGHT_WATER_TOWER_DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H
