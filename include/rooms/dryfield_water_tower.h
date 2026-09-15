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

/// Payload `Gp_DispatchMsg` carries for message 0x7DB, the record this room's
/// script table `D_dryfield_water_tower_80181B00` pairs with
/// `func_dryfield_water_tower_8017F808` next to its `Room_Util08` 0x7D4 entry.
/// Only the halfword at 0x2 is read; it becomes the receiving task's state. The
/// same four bytes the actors send in `Actor210600Msg` / `Actor560800Msg`, and
/// the shape `AcropolisBridgeMsg7DB` gives them on the receiving side.
typedef struct DwtwMsg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} DwtwMsg7DB;
STATIC_ASSERT_SIZEOF(DwtwMsg7DB, 0x4);

/// Scratch state of the room's cap script, stored at `Task::idMap`: the
/// 0x7C-byte block `func_dryfield_water_tower_8017F128` allocates for its own
/// task before it runs. The whole body is halfword slots -- the pairs are
/// timers the instructions above the state switch count down -- and the ones
/// the decomp has named so far are `field_58` / `field_5A` / `field_60`, the
/// three `func_dryfield_water_tower_8017F808` clears on message 0x7DB, and
/// `field_5C` / `field_5E`, which `func_dryfield_water_tower_8017F8E8` writes
/// together. `field_6C` / `field_6E` are 0/1 latches set by
/// `func_dryfield_water_tower_8017FBC8` / `8017FBD8`.
typedef struct DryfieldWaterTowerState {
    /* 0x00 */ u8  pad_0[0x58];
    /* 0x58 */ s16 field_58;
    /* 0x5A */ s16 field_5A;
    /* 0x5C */ s16 field_5C;
    /* 0x5E */ s16 field_5E;
    /* 0x60 */ s16 field_60;
    /* 0x62 */ u8  pad_62[0xA];
    /* 0x6C */ s16 field_6C;
    /* 0x6E */ s16 field_6E;
    /* 0x70 */ u8  pad_70[0xC];
} DryfieldWaterTowerState;
STATIC_ASSERT_SIZEOF(DryfieldWaterTowerState, 0x7C);

/// The water tower's script task, published by its state-0 init
/// `func_dryfield_water_tower_8017FD64`. `DwtwWork` hangs off its `idMap`.
extern Task* D_dryfield_water_tower_801876AC;

/// The water tower's cap-script task, published by
/// `func_dryfield_water_tower_8017F128`, which allocates the cap script's
/// 0x7C-byte scratch block into the task's `idMap` first.
extern Task* D_dryfield_water_tower_801876A4;

#endif // ROOMS_DRYFIELD_WATER_TOWER_H
