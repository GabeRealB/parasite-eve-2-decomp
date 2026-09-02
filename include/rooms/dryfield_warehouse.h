#ifndef ROOMS_DRYFIELD_WAREHOUSE_H
#define ROOMS_DRYFIELD_WAREHOUSE_H

#include "common.h"

#include "main/task.h"

/// Work block for the warehouse cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_warehouse_8017E090` and hung off `Task::idMap` (0x1C).
///
/// The room's overlay carries the same cutscene-task body as
/// `dryfield_gas_station` and `dryfield_water_tank`, so the layout is shared:
/// `owner` is the slot-3 game pointer (`Game_GetPtrSlot(3)`) the task dispatches
/// its messages to, `field_6` selects the script step
/// (`func_dryfield_warehouse_8017DBB0` branches on it), and `playerEffActive`
/// is the flag guarding `Gp_KillPlayerEffs`.
typedef struct DwhWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ s16   field_4;
    /* 0x06 */ s16   field_6;
    /* 0x08 */ byte  pad_8[0x4];
    /* 0x0C */ s16   playerEffActive;
    /* 0x0E */ byte  pad_E[0x2];
} DwhWork;
STATIC_ASSERT_SIZEOF(DwhWork, 0x10);

#endif // ROOMS_DRYFIELD_WAREHOUSE_H
