#ifndef ROOMS_DRYFIELD_WATER_TOWER_H
#define ROOMS_DRYFIELD_WATER_TOWER_H

#include "common.h"

#include "main/task.h"

/// Work block of the water tower's script task, allocated as 0x18 zeroed bytes
/// by `func_dryfield_water_tower_8017FD64` and hung off `Task::idMap` (0x1C) --
/// that slot is *not* a `TaskIdMap` here. Reach it with
/// `(DwtwWork*)task->idMap`.
///
/// The first three fields are the tasks the room's script dispatches its
/// messages to: `field_0` is the slot-3 game pointer (`Game_GetPtrSlot(3)`),
/// and `field_4` / `field_8` are `Gp_FindWorkById(...)->field_0` for two ids
/// built from the session's `field_6` / `field_7` bytes (the second id has
/// 0x1000 OR'd in). `func_dryfield_water_tower_80180220` sends the 0x7D4 pair
/// to `field_8` / `field_4` and the 0x3F3 / 0x3E9 messages to `field_0`.
///
/// `field_C` / `field_E` are written together as a pair -- `field_E` always
/// cleared -- by `func_dryfield_water_tower_80180174`, a body the breezeway
/// room carries as `func_dryfield_breezeway_8017E370`; their meaning is not yet
/// known. `field_14` is a 0/1 latch that lets
/// `func_dryfield_water_tower_80180194` dispatch its one-shot message once.
typedef struct DwtwWork {
    /* 0x00 */ Task* field_0; // Game_GetPtrSlot(3), Gp_DispatchMsg target
    /* 0x04 */ Task* field_4; // Gp_FindWorkById(...)->field_0
    /* 0x08 */ Task* field_8; // Gp_FindWorkById(...)->field_0
    /* 0x0C */ s16   field_C;
    /* 0x0E */ s16   field_E;
    /* 0x10 */ byte  pad_10[0x4];
    /* 0x14 */ s16   field_14;
    /* 0x16 */ byte  pad_16[0x2];
} DwtwWork;
STATIC_ASSERT_SIZEOF(DwtwWork, 0x18);

/// The water tower's script task, published by its state-0 init
/// `func_dryfield_water_tower_8017FD64`. `DwtwWork` hangs off its `idMap`.
extern Task* D_dryfield_water_tower_801876AC;

#endif // ROOMS_DRYFIELD_WATER_TOWER_H
