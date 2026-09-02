#ifndef ROOMS_DRYFIELD_WATER_TANK_H
#define ROOMS_DRYFIELD_WATER_TANK_H

#include "common.h"

#include "main/task.h"

/// Work block for the water-tank cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_water_tank_8017E9F8` and hung off `Task::idMap` (0x1C).
///
/// The room's overlay carries the same cutscene-task body as
/// `dryfield_gas_station` and `dryfield_warehouse`, so the layout is shared:
/// `owner` is the slot-3 game pointer (`Game_GetPtrSlot(3)`) the task dispatches
/// its messages to, and the two shorts at 0x4 are script parameters written
/// together by `func_dryfield_water_tank_8017EB80`.
typedef struct DwtWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ s16   field_4;
    /* 0x06 */ s16   field_6;
    /* 0x08 */ byte  pad_8[0x4];
    /* 0x0C */ s16   playerEffActive;
    /* 0x0E */ byte  pad_E[0x2];
} DwtWork;
STATIC_ASSERT_SIZEOF(DwtWork, 0x10);

#endif // ROOMS_DRYFIELD_WATER_TANK_H
