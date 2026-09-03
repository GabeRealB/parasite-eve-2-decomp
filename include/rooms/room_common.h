#ifndef ROOMS_ROOM_COMMON_H
#define ROOMS_ROOM_COMMON_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"

/// Gameplay-side action-prompt state, imported by room overlays as the unnamed
/// data symbol `D_80114D28` (the gameplay symbol map has no name for it yet).
///
/// A room's hotspot scan stores the id of the thing under the cursor in
/// `targetId` and a mode in `mode` (0 = nothing under the cursor, 1 = a hotspot
/// is highlighted, 2 = the hotspot is confirmed). `screenX` / `screenY` are the
/// coordinates handed to `func_800D4E78`, which parks them in the gameplay
/// globals the prompt's display task reads. Fields no room overlay touches are
/// left as padding.
typedef struct RoomActionPrompt {
    /* 0x00 */ byte pad_0[0x8];
    /* 0x08 */ s16  screenX;
    /* 0x0A */ s16  screenY;
    /* 0x0C */ s16  targetId;
    /* 0x0E */ byte pad_E[0x2];
    /* 0x10 */ u8   mode;
    /* 0x11 */ byte pad_11[0x3];
    /* 0x14 */ u16  field_14;
    /* 0x16 */ byte pad_16[0x6];
    /* 0x1C */ u16  field_1C;
    /* 0x1E */ byte pad_1E[0x2];
} RoomActionPrompt;

extern RoomActionPrompt D_80114D28;

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

/// Per-item shop stock row (`D_8010E138`, the tail of `Gp_QtyById0` covering
/// item ids 0xA0-0xBF). `perBuy` is how many units one purchase adds and
/// `maxHeld` the ceiling the shop will stock the player up to, so the row
/// handler offers at most `(maxHeld - held + perBuy - 1) / perBuy` purchases.
typedef struct RoomShopStock {
    /* 0x00 */ u8   perBuy;
    /* 0x01 */ byte pad_1[1];
    /* 0x02 */ u16  maxHeld;
} RoomShopStock;
STATIC_ASSERT_SIZEOF(RoomShopStock, 0x4);

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

/// 0xC4 work block the "Play Data" item-usage panel allocates and parks in
/// `Task::idMap`. The builder walks item ids 0x80-0x9F, keeps the ones the save
/// has a non-zero use count for, and fills three parallel arrays indexed by the
/// row the list is drawing: the item id, the share of all recorded uses in
/// hundredths of a percent (0-10000, printed as `NN.NN%`), and the width of the
/// row's gauge as a 12-bit fraction of the panel's inner width. The tail of the
/// allocation is unused.
typedef struct RoomItemUsage {
    /* 0x00 */ s16  itemIds[0x20];
    /* 0x40 */ s16  percents[0x20];
    /* 0x80 */ s16  barWidths[0x20];
    /* 0xC0 */ byte pad_C0[0x4];
} RoomItemUsage;
STATIC_ASSERT_SIZEOF(RoomItemUsage, 0xC4);

/// 0xC4 work block the "Play Data" PE-usage panel allocates and parks in
/// `Task::idMap`, laid out exactly like `RoomItemUsage`. The builder walks the
/// twelve Parasite Energy slots, keeps the ones the save has a non-zero use
/// count for, and fills three parallel arrays indexed by the row the list is
/// drawing: the id of the slot's known level, that slot's share of all recorded
/// uses in hundredths of a percent (0-10000, printed as `NN.NN%`), and the
/// width of the row's gauge as a 12-bit fraction of the panel's inner width.
/// The tail of the allocation is unused.
typedef struct RoomPeUsage {
    /* 0x00 */ s16  peIds[0x20];
    /* 0x40 */ s16  percents[0x20];
    /* 0x80 */ s16  barWidths[0x20];
    /* 0xC0 */ byte pad_C0[0x4];
} RoomPeUsage;
STATIC_ASSERT_SIZEOF(RoomPeUsage, 0xC4);

// =============================================================================
// Functions — shared room library (src/rooms/lib)
// =============================================================================

s32  Room_Util18(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3);
void Room_Script21(Task* task);

/// Fills the "Play Data" item-usage panel's `RoomItemUsage` block from the
/// save's per-item use counters, then seeds `list` with the row count.
void RoomsShared80180c98(UiList* list, UiObject* obj);

/// Fills the "Play Data" PE-usage panel's `RoomPeUsage` block from the save's
/// per-slot Parasite Energy use counters, then seeds `list` with the row count.
void RoomsShared80180f94(UiList* list, UiObject* obj);

/// Appends `item` to the shop list the panel's task owns, keeping one entry per
/// item kind (a higher level of the same kind replaces the entry it finds).
void RoomsShared8017e3f4(RoomShopList* shop, UiObject* obj, s32 item);

#endif // ROOMS_ROOM_COMMON_H
