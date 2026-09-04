#ifndef ROOMS_ROOM_COMMON_H
#define ROOMS_ROOM_COMMON_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"
#include "main/ui.h"

/// On-screen position of an action prompt's cursor. `Room_UtilCursor` compares
/// the pair against a previously latched copy as a single word to decide whether
/// the cursor has moved since the last press, so the two shorts are reachable
/// both individually and as one `s32`.
typedef struct RoomActionPromptPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
} RoomActionPromptPos;

typedef union RoomActionPromptScreen {
    /* 0x0 */ RoomActionPromptPos xy;
    /* 0x0 */ s32                 packed;
} RoomActionPromptScreen;

/// One of the two button slots at the tail of `RoomActionPrompt`. `state` is the
/// press classification the cursor task writes each frame (0 none, 1 held,
/// 2 pressed, 3 released, 4 double-press), `heldFrames` counts the frames since
/// the slot was last armed and `lastPos` latches the cursor position of the
/// previous press so a double-press only registers when the cursor has not
/// moved. Slot 0 watches the confirm mask (0x40) and slot 1 the cancel mask
/// (0xA0).
typedef struct RoomActionPromptButton {
    /* 0x0 */ u16 state;
    /* 0x2 */ u16 heldFrames;
    /* 0x4 */ s32 lastPos;
} RoomActionPromptButton;
STATIC_ASSERT_SIZEOF(RoomActionPromptButton, 0x8);

/// Gameplay-side action-prompt state, imported by room overlays as the unnamed
/// data symbol `D_80114D28` (the gameplay symbol map has no name for it yet).
/// There are two of them, one per pad port.
///
/// A room's hotspot scan stores the id of the thing under the cursor in
/// `targetId` and a mode in `mode` (0 = nothing under the cursor, 1 = a hotspot
/// is highlighted, 2 = the hotspot is confirmed). `screen` holds the
/// coordinates handed to `func_800D4E78`, which parks them in the gameplay
/// globals the prompt's display task reads; `field_0` / `field_4` are the same
/// position in 1/512-pixel fixed point, which is what the analog stick and the
/// d-pad actually integrate into. `targetId` doubles as the cursor speed and
/// `field_E` as the double-press window. Fields no room overlay touches are
/// left as padding.
typedef struct RoomActionPrompt {
    /* 0x00 */ s32                    field_0;
    /* 0x04 */ s32                    field_4;
    /* 0x08 */ RoomActionPromptScreen screen;
    /* 0x0C */ s16                    targetId;
    /* 0x0E */ u16                    field_E;
    /* 0x10 */ u8                     mode;
    /* 0x11 */ byte                   pad_11[0x3];
    /* 0x14 */ RoomActionPromptButton buttons[2];
} RoomActionPrompt;
STATIC_ASSERT_SIZEOF(RoomActionPrompt, 0x24);

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
    /* 0x28 */ u16     field_28;
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

/// The `GsCOORDINATE2` at `TmdObject::field_8` as the room overlays see it: the
/// game stores the placed object's Euler angles in the `SVECTOR` that overlays
/// libgs's `param` / `super` slots (0x44-0x4B) and rebuilds `coord` from it
/// with `RotMatrix`. `sub` is the parent coordinate (`GsCOORDINATE2::sub`).
typedef struct RoomCoord {
    /* 0x00 */ u32            flg;
    /* 0x04 */ MATRIX         coord;
    /* 0x24 */ MATRIX         workm;
    /* 0x44 */ SVECTOR        rot;
    /* 0x4C */ GsCOORDINATE2* sub;
} RoomCoord;
STATIC_ASSERT_SIZEOF(RoomCoord, 0x50);

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

/// 0xAC work block the mirror-reflection task keeps at `Task::idMap`. Rooms
/// with a reflective surface (the Acropolis elevator halls and square, motel
/// room 6, the Neo Ark observatory) spawn a task that re-attaches the player's
/// own TMD source and draws it through `coord`, which is `Gfx_ViewCoord` with
/// one GTE rotation column negated.
///
/// `viewFlg` caches `Gfx_ViewCoord.flg & 0x7FFFFFFF` so the mirror only rebuilds
/// its matrices when the view moves, and `field_4` marks the block as live;
/// both are set to their "dirty" values (`-1` / `0`) as the task starts so the
/// first frame always rebuilds. `light` and `color` are the matrices hung off
/// the clone's `TmdObject`, `field_A0` is the screen-space clip rectangle
/// (-160, 160, -120, 120) and `configRev` caches `Wip_SysConfig.field_21`.
typedef struct RoomMirrorWork {
    /* 0x00 */ s32           viewFlg;
    /* 0x04 */ s32           field_4;
    /* 0x08 */ s32           field_8;
    /* 0x0C */ s32           field_C;
    /* 0x10 */ GsCOORDINATE2 coord;
    /* 0x60 */ MATRIX        light;
    /* 0x80 */ MATRIX        color;
    /* 0xA0 */ s16           field_A0[4];
    /* 0xA8 */ s32           configRev;
} RoomMirrorWork;
STATIC_ASSERT_SIZEOF(RoomMirrorWork, 0xAC);

/// 8-byte message record a room's message handlers receive alongside the
/// request. Handlers registered in a room's `(msgId, handler)` dispatch table
/// are passed the incoming record and an outgoing copy of it, and answer by
/// editing `field_3` of the copy. `field_5` non-zero suppresses the side
/// effects (the handler only reports what *would* happen); `field_6` is the
/// nibble index passed to `Gp_SetNibbleIf`. Alignment is 2, which is why the
/// whole-record copies compile to `lwl`/`lwr` pairs.
typedef struct _RoomEventMsg {
    /* 0x0 */ u16 msgId;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ s8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
    /* 0x6 */ u16 field_6;
} RoomEventMsg;
STATIC_ASSERT_SIZEOF(RoomEventMsg, 0x8);

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

/// 0x14-byte scratch block an elevator hall's light-shaft task takes from
/// `G_SCRATCH_HEAD`. `vec` is the
/// shaft's world position copied out of the task's `GsCOORDINATE2`
/// (`workm.t`) and pushed through `GsWSMATRIX` with a single `RTPS` - but the
/// screen point is kept too: `sx` / `sy` are the projected centre and `halfWidth`
/// the shaft's on-screen half width, `spawnArg1`'s high byte scaled by `1 / otz`
/// so the two `POLY_G4` halves narrow with distance.
typedef struct _RoomShaftScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     halfWidth;
    /* 0x08 */ SVECTOR vec;
    /* 0x10 */ u16     sx;
    /* 0x12 */ u16     sy;
} RoomShaftScratch;
STATIC_ASSERT_SIZEOF(RoomShaftScratch, 0x14);

/// Overlay of `Task::spawnArg1` for that task: `phase` steps the shaft's
/// pulsing red channel off the global frame counter, `height` is the length
/// the two halves are drawn at before the `1 / otz` divide.
typedef struct _RoomShaftArg {
    /* 0x0 */ u8   phase;
    /* 0x1 */ u8   height;
    /* 0x2 */ byte pad_2[2];
} RoomShaftArg;
STATIC_ASSERT_SIZEOF(RoomShaftArg, 0x4);

/// 0x24-byte scratch block a room's ambient-mote task takes from
/// `G_SCRATCH_HEAD`. `v` holds the four corners of the unit quad `D_80111E38`,
/// scaled to the mote's half-size, rotated by the mote's own `GsCOORDINATE2`
/// and then projected through `GsWSMATRIX`; `otz` is the `gte_stszotz` of that
/// projection, which both rejects motes closer than 0x11 and picks the OT
/// bucket the `POLY_FT4` is linked into.
typedef struct _RoomQuadScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ SVECTOR v[4];
} RoomQuadScratch;
STATIC_ASSERT_SIZEOF(RoomQuadScratch, 0x24);

#endif // ROOMS_ROOM_COMMON_H
