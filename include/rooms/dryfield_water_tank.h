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

/// Payload of the room broadcast 0x7DB, the record `Gp_DispatchMsg` hands its
/// handler as `arg2`. The same four bytes the other rooms' 0x7DB handlers read;
/// this room only ever looks at the halfword at 0x2, which carries the script
/// state the receiver moves to. The driver sends it with the payload staged on
/// its own frame, only `field_2` filled in.
typedef struct DwtMsg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} DwtMsg7DB;
STATIC_ASSERT_SIZEOF(DwtMsg7DB, 0x4);

/// Work block of the water-tank room's script-driver task, a
/// `Mem_Malloc(0x58, 0)` the driver `func_dryfield_water_tank_8017DEA4` hangs
/// off `Task::idMap` (0x1C). That task is also parked in
/// `D_dryfield_water_tank_80188D4C`, which is how the sibling entry points
/// `func_dryfield_water_tank_8017E194` and `..._8017E1B4` reach this block.
///
/// `owner` is `Game_GetPtrSlot(3)`, the task every `Gp_DispatchMsg` in the
/// driver targets; `child` is the task spawned from
/// `D_dryfield_water_tank_8017FF88`, the one messages 0x7D4 / 0x7D5 / 0x7DB are
/// sent to. `field_50` is a request the driver's per-frame switch consumes and
/// clears, and `func_dryfield_water_tank_8017E194` is what sets it.
/// `field_4C`, `field_52` and `field_54` have no identified role yet.
///
/// Distinct from `DwtWork`: that one belongs to the cutscene task named by
/// `RoomsShared80180b2cTask`.
typedef struct DwtScriptWork {
    /* 0x00 */ byte  pad_0[0x40];
    /* 0x40 */ Task* owner;
    /* 0x44 */ Task* child;
    /* 0x48 */ byte  pad_48[0x4];
    /* 0x4C */ s16   field_4C;
    /* 0x4E */ byte  pad_4E[0x2];
    /* 0x50 */ s16   field_50;
    /* 0x52 */ s16   field_52;
    /* 0x54 */ s16   field_54;
    /* 0x56 */ byte  pad_56[0x2];
} DwtScriptWork;
STATIC_ASSERT_SIZEOF(DwtScriptWork, 0x58);

/// Toggle the room's cutscene-“played” state: `arg0 != 0` marks the task the
/// script driver points at as watched and clears the sibling flag, `arg0 == 0`
/// does the opposite. `func_dryfield_water_tank_8017DB48` passes the game-flag
/// `0x55` nibble through it, one way per value.
void func_dryfield_water_tank_8017EFF4(s32 arg0);

#endif // ROOMS_DRYFIELD_WATER_TANK_H
