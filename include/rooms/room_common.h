#ifndef ROOMS_ROOM_COMMON_H
#define ROOMS_ROOM_COMMON_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

#include <psyq/libgte.h>

/// Work object a `Gp_State1C`-pool room effect task keeps at `Task::spawnArg2`
/// (released with `Gp_ReleaseState1CMem`). `field_10` is the `SVECTOR` handed to
/// `Gp_SpawnEff`; `field_22` is the per-frame tick the task compares against its
/// lifetime; `field_24` / `field_26` are the animated colour and size ramps and
/// `field_2A` the per-frame step derived from `Task::spawnArg1`.
///
/// Shared by every room overlay that spawns effects this way
/// (`shelter_b6_nursery`, `acropolis_fire_escape`, …).
typedef struct RoomEffWork {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR field_10;
    /* 0x18 */ byte    pad_18[0xA];
    /* 0x22 */ u16     field_22;
    /* 0x24 */ u16     field_24;
    /* 0x26 */ u16     field_26;
    /* 0x28 */ byte    pad_28[2];
    /* 0x2A */ u16     field_2A;
} RoomEffWork;

/// Position + Euler rotation handed to `Room_Util18`, which copies it onto a
/// task's `TmdObject` coordinate frame (`Task::extra->field_8`): the three
/// longs become the coordinate's translation and the three shorts are fed to
/// `RotMatrixZYX`. Room overlays keep one of these per placed object in their
/// own `.data`.
typedef struct RoomPlacement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} RoomPlacement;

/// 0x20 work block a room's "show a two-line message" task allocates and parks
/// in `Task::idMap`: a `TextBlockDesc` handed to `Ui_SpawnTextBlock` followed by
/// the two `TextLineNode`s the descriptor's list points at, so one allocation
/// carries both. The room picks which pair of strings to publish from
/// `Task::spawnArg1`.
typedef struct RoomTextBlock {
    /* 0x00 */ TextBlockDesc desc;
    /* 0x0C */ u8            field_C;
    /* 0x0D */ byte          pad_D[3];
    /* 0x10 */ TextLineNode  lines[2];
} RoomTextBlock;
STATIC_ASSERT_SIZEOF(RoomTextBlock, 0x20);

/// View of the task that owns a shop / vending-machine panel, used by the row
/// handlers in the parking and shelter rooms. `Task` declares offset 0x34 as a
/// single `s32 spawnArg1`, but the shop tasks keep a mode in its upper halfword
/// and read that halfword on its own, so the handlers reach the mode through
/// this view of the same task rather than through `Task`.
typedef struct RoomShopTask {
    /* 0x00 */ byte pad_0[0x36];
    /* 0x36 */ s16  mode;
} RoomShopTask;

/// 0xA4 work block a shop / vending-machine panel task allocates and parks in
/// `Task::idMap`: the `UiList` the panel is drawn from, followed by the ids of
/// the items the room currently offers. The overlay's list builder fills
/// `items` while counting them into `list.field_4`, then sorts that prefix in
/// place, so one allocation carries both the list state and its contents.
typedef struct RoomShopList {
    /* 0x00 */ UiList list;
    /* 0x24 */ u16    items[0x40];
} RoomShopList;
STATIC_ASSERT_SIZEOF(RoomShopList, 0xA4);

// =============================================================================
// Functions — shared room library (src/rooms/lib)
// =============================================================================

s32  Room_Util18(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3);
void Room_Script21(Task* task);

#endif // ROOMS_ROOM_COMMON_H
