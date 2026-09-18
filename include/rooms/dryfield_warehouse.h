#ifndef ROOMS_DRYFIELD_WAREHOUSE_H
#define ROOMS_DRYFIELD_WAREHOUSE_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Work block for the warehouse cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_warehouse_8017E090` and hung off `Task::work` (0x1C).
///
/// The room's overlay carries the same cutscene-task body as
/// `dryfield_gas_station` and `dryfield_water_tank`, so the layout is shared:
/// `owner` is the slot-3 game pointer (`Game_GetPtrSlot(3)`) the task dispatches
/// its messages to, `field_6` selects the script step
/// (`func_dryfield_warehouse_8017DBB0` branches on it), and `playerEffActive`
/// is the flag guarding `Gp_SpawnWeaponEff` / `Gp_KillPlayerEffs`.
typedef struct DwhWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ s16   field_4;
    /* 0x06 */ s16   field_6;
    /* 0x08 */ byte  pad_8[0x4];
    /* 0x0C */ u16   playerEffActive;
    /* 0x0E */ byte  pad_E[0x2];
} DwhWork;
STATIC_ASSERT_SIZEOF(DwhWork, 0x10);

/// The 8-byte fade block `func_dryfield_warehouse_8017E308` allocates with
/// `Mem_Malloc(8, 0)` on its first tick and parks in `Task::work`: two bytes of
/// padding, then the three halfword channels `Fade_DrawOverlay` draws. All three
/// start at 0 and are walked *up* by `Task::spawnArg1` each frame; `r` is the one
/// the end-of-fade test watches. Same layout as the actors family's
/// `ActorShared801344acWork`, which walks the channels down instead.
typedef struct DwhFadeWork {
    /* 0x0 */ u8  pad_0[0x2];
    /* 0x2 */ s16 r;
    /* 0x4 */ s16 g;
    /* 0x6 */ s16 b;
} DwhFadeWork;
STATIC_ASSERT_SIZEOF(DwhFadeWork, 0x8);

/// The screen-fade task the warehouse's script spawns and keeps the handle of.
/// `func_dryfield_warehouse_8017DBB0` stores the task `Task_SpawnFromTable`
/// returns, `func_dryfield_warehouse_8017E308` - that task itself - clears it
/// once the fade has run its course, and `func_dryfield_warehouse_8017DA58`
/// kills whatever handle is still there before fading back in.
extern Task* D_dryfield_warehouse_801821C0;

s32 func_dryfield_warehouse_8017D764(s32 arg0, s32 arg1, s32 arg2);
s32 func_dryfield_warehouse_8017D824(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

#endif // ROOMS_DRYFIELD_WAREHOUSE_H
