#ifndef ROOMS_DRYFIELD_GAS_STATION_H
#define ROOMS_DRYFIELD_GAS_STATION_H

#include "common.h"

#include "main/task.h"

/// Work block for the gas-station cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_gas_station_801807E0` and hung off `Task::idMap` (0x1C).
///
/// `owner` is the slot-3 game pointer (`Game_GetPtrSlot(3)`) the task dispatches
/// its messages to, and `playerEffActive` is the flag guarding
/// `Gp_KillPlayerEffs` / `Gp_SpawnWeaponEff`. The two shorts at 0x4 are script
/// parameters written together by `func_dryfield_gas_station_80180B2C`; their
/// meaning is not yet known.
typedef struct DgsWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ s16   field_4;
    /* 0x06 */ s16   field_6;
    /* 0x08 */ byte  pad_8[0x4];
    /* 0x0C */ s16   playerEffActive;
    /* 0x0E */ byte  pad_E[0x2];
} DgsWork;
STATIC_ASSERT_SIZEOF(DgsWork, 0x10);

#endif // ROOMS_DRYFIELD_GAS_STATION_H
