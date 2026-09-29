#ifndef DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H
#define DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/message.h"

#include "common.h"

// Callbacks referenced by the overlay's shared data tables.
void func_dryfield_night_water_tower_8017D770(Task *);
s32 func_dryfield_night_water_tower_8017D8E0(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_night_water_tower_8017DA4C(Task *, s32, s32, GpMessageArg);
s32 func_dryfield_night_water_tower_8017DA9C(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_night_water_tower_8017DAA4(Task *, s32, s32, GpMessageArg);
s32 func_dryfield_night_water_tower_8017DAD4(Task *, s32, GpMessageArg, GpMessageArg);

#endif // DRYFIELD_NIGHT_WATER_TOWER_PRIVATE_H
