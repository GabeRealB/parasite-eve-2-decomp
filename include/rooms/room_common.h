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
/// `field_2A` the per-frame step derived from `Task::spawnArg1`. A task that
/// drifts its own coordinate frame also uses `field_18` / `field_1A` /
/// `field_1C` as the velocity the spawner staged for `field_10`, and `field_20`
/// as the animation step it advances every `field_28` ticks.
///
/// Shared by every room overlay that spawns effects this way
/// (`shelter_b6_nursery`, `acropolis_fire_escape`, …).
typedef struct RoomEffWork {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR field_10;
    /* 0x18 */ u16     field_18;
    /* 0x1A */ u16     field_1A;
    /* 0x1C */ u16     field_1C;
    /* 0x1E */ byte    pad_1E[0x2];
    /* 0x20 */ u16     field_20;
    /* 0x22 */ u16     field_22;
    /* 0x24 */ u16     field_24;
    /* 0x26 */ u16     field_26;
    /* 0x28 */ u16     field_28;
    /* 0x2A */ u16     field_2A;
} RoomEffWork;

/// Position + Euler rotation handed to `Room_Util08` / `Room_Util18`, which
/// copy it onto a task's `TmdObject` coordinate frame (`Task::extra->field_8`):
/// the three longs become the coordinate's translation and the three shorts
/// are the Euler angles (`Room_Util08` applies Y then X then Z via
/// `Gfx_RotMatrix*`; `Room_Util18` feeds them to `RotMatrixZYX`). Room overlays
/// keep one of these per placed object in their own `.data`.
typedef struct RoomPlacement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} RoomPlacement;

/// Screen rectangle outlined by `Room_Draw26`: the corners it draws are
/// (`x`, `y`) and (`x + w`, `y + h`), so `w` and `h` are extents rather than a
/// second corner. The fields are unsigned because the drawer loads every one
/// of them with `lhu`.
typedef struct RoomRect {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
    /* 0x4 */ u16 w;
    /* 0x6 */ u16 h;
} RoomRect;
STATIC_ASSERT_SIZEOF(RoomRect, 0x8);

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

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void Room_Util01(u8* str, s32 decimals);
/// Resets both action-prompt slots before a script's first cursor scan and
/// steps the caller on one state.
void Room_Util04(Task* task);
/// Steps the caller on one state and does nothing else; state tables use it as
/// a one-frame filler.
void Room_Util26(Task* task);
void Room_Util08(Task* task, s32 arg1, RoomPlacement* placement);
s32  Room_Util18(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3);
void Room_Util19(Task* task, s32 arg1, s32 arg2);
void Room_SaveUi01(Task* task);
/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when `gte_stflg` is
/// non-negative, queues one semi-transparent `POLY_FT4` (tpage 0x2B, clut
/// `(arg1 & 0x3F) | 0x4380`). `arg1` selects the 40-texel UV column
/// `(s16)arg1 * 40` at v=0..0x27. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 39 / otz`. RGB is
/// `((field_8 & 1) * 16) + 0x20` on all three channels. Same 0x10 scratch
/// layout as `Room_Draw13`.
void Room_Draw17(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when the OTZ is at least
/// 0x11, queues one semi-transparent `POLY_FT4` (tpage 0x2B, clut
/// `(arg1 & 0x3F) | 0x4380`). `arg1` selects the 40-texel UV column
/// `(s16)arg1 * 40` at v=0..0x27. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 39 / otz`. RGB is
/// `((field_8 & 1) * 16) + 0x20` on all three channels. Same 0xC scratch
/// layout as `Room_Draw25`.
void Room_Draw20(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Handwritten GTE routine. Projects the world-space point `arg0` through
/// `Gfx_ViewWorldMtx` and emits screen-aligned quads there; `arg1` is a signed
/// half-extent divided by the projected depth and `arg2` packs the primitive's
/// page/blend bits.
void Room_Draw21(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when the OTZ is at least
/// 0x11, queues four gouraud `POLY_G4` wedges around the projected centre.
/// `arg1` is a signed half-extent; the on-screen radius is `arg1 * 64 / otz`.
void Room_Draw25(SVECTOR* arg0, s16 arg1);
/// Same four-wedge gouraud disc as `Room_Draw25` (same 0xC scratch layout),
/// but `arg2` scales the inner vertex by the frame-counter blend byte
/// `((field_8 & 1) * 8 | 0x20)`: red is `blend * ((arg2 << 16) >> 24)`,
/// green `blend * (((arg2 << 16) >> 20) & 1)`, blue `blend * (arg2 & 1)`.
void Room_Draw30(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Same body as `Room_Draw30`, but green is `blend * (((arg2 << 16) >> 20) & 3)`
/// and blue `blend * (arg2 & 3)`.
void Room_Draw29(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when `gte_stflg` is
/// non-negative, queues four gouraud `POLY_G4` wedges around the projected
/// centre. `arg1` is a signed half-extent; the on-screen radius is
/// `(s16)arg1 * 64 / otz`. `arg2` packs three RGB nibbles for the lit inner
/// vertex, OR'd with the frame-counter blend bit so each wedge fades to black.
void Room_Draw13(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Same four-wedge gouraud disc as `Room_Draw13`, but the scratch block keeps
/// `radius` at 0x4 and `flag` at 0x8.
void Room_Draw31(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when `gte_stflg` is
/// non-negative, queues two gouraud `POLY_G4` diamonds and two gouraud
/// `LINE_G3` diagonals around the projected centre. Same 0x10 scratch layout
/// as `Room_Draw13`. `arg2` is a signed half-extent; the on-screen radius is
/// `(s16)arg2 * 32 / otz`. `arg1` scales `Display_State.field_8` into `rsin`
/// so the lit vertex pulses as `rsin(...) / 34 + 0x78` on red.
void Room_Draw38(SVECTOR* arg0, s16 arg1, s32 arg2);
/// Same two-diamond gouraud plus two `LINE_G3` diagonals as `Room_Draw38`
/// (same 0x10 scratch layout), but the lit vertex pulses as
/// `rsin(...) / 34 + 0x78` on green and blue rather than red.
void Room_Draw18(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Same two-diamond gouraud plus two `LINE_G3` diagonals as `Room_Draw18`
/// (same 0x10 scratch layout), but the on-screen radius is
/// `(s16)arg2 * 48 / otz` rather than `* 32`.
void Room_Draw32(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when `gte_stflg` is
/// non-negative, queues a sixteen-wedge gouraud disc plus two inner cross
/// wedges around the projected centre. `arg2` is a signed half-extent;
/// on-screen radii are `(s16)arg2 * 64 / otz` (outer) and `(s16)arg2 * 8 / otz`
/// (inner). `arg1` scales `Display_State.field_8` into `rsin` so the lit vertex
/// pulses as `rsin(...) / 34 + 0x78` on green and blue.
void Room_Draw05(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Handwritten GTE routine. Draws an eight-segment gouraud ring centred on
/// `arg0`'s world position (`workm.t` through `GsWSMATRIX`). `arg1` is the
/// radius in world units (scaled by 64 and divided by the projected OTZ after
/// it is incremented) and `arg2` the RGB triple, which only lights the ring's
/// inner vertex so each `POLY_G4` fades to black. Dropped when `gte_stflg` is
/// negative.
void Room_Draw10(GsCOORDINATE2* arg0, s32 arg1, u8* arg2);
/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// `gte_stflg` is non-negative, queues one semi-transparent `POLY_FT4` (tpage
/// 0x2A, clut 0x42CB). `arg1` selects one of four 24-texel UV columns
/// `(arg1 & 3) * 24 + 0x60` at v=0..0x17. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 23 / (otz + 1)`. `arg3` is the RGB on all
/// three channels. Dropped when `gte_stflg` is negative.
void Room_Draw14(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
/// Projects two world-space points (`arg0` and `arg0 + 1`) through
/// `Gfx_ViewWorldMtx` and, when the second OTZ is at least 0x11, queues
/// gouraud `POLY_G4` wedges: a half-disc at each projected centre plus
/// connecting slices. `arg1` is a signed half-extent; on-screen radii are
/// `(s16)arg1 * 64 / otz`. `arg2` is a signed angle offset. The first OTZ is
/// clamped to 0x10.
void Room_Draw11(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Same two-point gouraud wedges as `Room_Draw11`, but the inner vertex is
/// scaled by the frame-counter blend byte `((field_8 & 1) * 8 | 0x20)`: red
/// is `blend * ((arg3 << 16) >> 24)`, green `blend * (((arg3 << 16) >> 20) & 1)`,
/// blue `blend * (arg3 & 1)`. `arg2` is still the signed angle offset.
void Room_Draw12(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
/// Same two-point gouraud wedges as `Room_Draw11`, but the sweep is anchored to
/// the screen-space angle between the two projected centres (`ratan2` of their
/// delta) instead of an `arg2` offset, both `gte_stflg` results are tested and
/// neither OTZ is clamped. The lit vertex takes the frame-counter blend byte
/// `((field_8 & 1) * 16) | 0x20`.
void Room_Draw08(SVECTOR* arg0, s32 arg1);
/// Same two-point gouraud wedges as `Room_Draw08` -- the sweep is anchored to
/// the `ratan2` of the two projected centres and both `gte_stflg` results are
/// tested -- but the lit vertex is coloured from `arg2` instead of a grey ramp:
/// each of the three nibbles of `arg2` moves into the high nibble of a channel
/// and is OR'd with the frame-counter blend bit `(field_8 & 1) * 8`.
void Room_Draw01(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Same two-point gouraud wedges as `Room_Draw12`, but the green/blue masks
/// are `& 3` rather than `& 1`.
void Room_Draw33(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
/// Same two-point gouraud wedges as `Room_Draw11`, but the inner vertex's RGB
/// is `((field_8 & 1) * 16) | 0x20` rather than `* 8`.
void Room_Draw34(SVECTOR* arg0, s32 arg1, s32 arg2);
/// Draws seven gouraud `POLY_G4` quads connecting two eight-slot coordinate
/// trails (`arg0` and `arg1`), walking backwards from `arg2`. Each quad is
/// the `workm.t` of two adjacent slots on both trails, projected through
/// `GsWSMATRIX`. The leading edge is scaled by `0x40 - 9 * i` and the
/// trailing edge by nine less. `arg3` packs three 2-bit RGB channels at bits
/// 8, 4 and 0. Dropped when `gte_stflg` is negative.
void Room_Draw03(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
/// Same eight-wedge gouraud ring as `Room_Draw10`, but the scratch block keeps
/// `radius` at 0xC and `flag` at 0x10. `arg1` is still the signed half-extent
/// `(s16)arg1 * 64 / (otz + 1)`, and `arg2` still tints only the inner vertex.
void Room_Draw04(GsCOORDINATE2* arg0, s32 arg1, u8* arg2);
/// Same sixteen-wedge gouraud ring as `Room_Draw09`, but the scratch block
/// keeps `flag` at 0xC with `rOuter`/`rInner` at 0x10/0x14. `arg1` is the
/// inner half-extent and `arg2` the extra outer width.
void Room_Draw07(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* arg3);
/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// `gte_stflg` is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a two-ring billboard. `arg1` is a signed half-extent; on-screen radii
/// are `(s16)arg1 * 64 / (otz + 1)` (outer) and `(s16)arg1 * 8 / (otz + 1)`
/// (inner). The RGB triple tints the inner vertex of the inner ring at full
/// brightness and the outer ring at half, so each wedge fades to a black rim.
void Room_DrawBillboard(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);
/// Queues a gouraud-shaded rectangle relative to a UI panel. Origin is
/// `field_20`/`field_22` plus (`arg1`, `arg2`); `arg3`/`arg4` are width and
/// height. Left vertices take `arg5`, right vertices take `arg6`.
void Room_Draw22(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6);
/// Outlines `rect` on screen in (`r`, `g`, `b`) with four unconnected flat
/// `LINE_F2`s -- top, right, bottom and left -- each linked into
/// `Gpu_CurrentOt[1]`.
void Room_Draw26(RoomRect* rect, u8 r, u8 g, u8 b);
/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// `gte_stflg` is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x43D3) rotated about the projected centre.
/// `arg1` selects the 32-texel UV column `(s16)arg1 << 5` at v=0xE0..0xFF.
/// `arg2` is a signed half-extent; the on-screen radius is
/// `(s16)arg2 * 31 / otz`. `arg3` is the spin angle, applied at `arg3` and
/// `arg3 + 0x400` through `rsin`/`rcos`.
void Room_Draw27(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
/// Same rotated shade-tex `POLY_FT4` as `Room_Draw27` (same 0x1C scratch
/// layout, tpage 0x2B, clut 0x43D3), but `arg1` selects the UV column
/// `(arg1 & 0xFFFF) << 5` rather than `(s16)arg1 << 5`.
void Room_Draw19(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
/// Append a 15-bit ABR-1 `DR_TPAGE` for VRAM origin (`tpage`, `arg1`) to OT
/// slot 8.
void Room_Draw42(s32 tpage, s16 arg1);
/// Queues the room's 16x24 action-prompt cursor icon at (`x`, `y`) into the head
/// of the current OT; `variant` selects the palette and 0 draws nothing.
void Room_Draw36(s32 x, s32 y, s32 variant);
void Room_Script10(Task* task);
void Room_Script11(Task* task);
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

/// 0xC-byte scratch block `Room_Draw20`, `Room_Draw25`, `Room_Draw29` and
/// `Room_Draw30` take from `G_SCRATCH_HEAD`. `otz` is the `gte_stszotz` of
/// `arg0` through `Gfx_ViewWorldMtx`; `sx`/`sy` are that screen point and
/// `radius` is `(s16)arg2 * 39 / otz` for `Room_Draw20` or `(s16)arg1 * 64 /
/// otz` for the gouraud discs, the on-screen half-extent of the primitive.
typedef struct _RoomDraw25Scratch {
    /* 0x00 */ s32 otz;
    /* 0x04 */ s32 radius;
    /* 0x08 */ u16 sx;
    /* 0x0A */ u16 sy;
} RoomDraw25Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw25Scratch, 0xC);

/// 0x10-byte scratch block `Room_Draw13`, `Room_Draw17`, `Room_Draw18`,
/// `Room_Draw32` and `Room_Draw38` take from `G_SCRATCH_HEAD`. `otz` is the
/// `gte_stszotz` of `arg0` through `Gfx_ViewWorldMtx`, `flag` is `gte_stflg`
/// (the primitives are dropped when it is negative), `radius` is
/// `(s16)arg1 * 64 / otz` for `Room_Draw13`, `(s16)arg2 * 39 / otz` for
/// `Room_Draw17`, `(s16)arg2 * 32 / otz` for `Room_Draw18`/`Room_Draw38` and
/// `(s16)arg2 * 48 / otz` for `Room_Draw32`, and `sx`/`sy` are the projected
/// centre.
typedef struct _RoomDraw13Scratch {
    /* 0x00 */ s32 otz;
    /* 0x04 */ s32 flag;
    /* 0x08 */ s32 radius;
    /* 0x0C */ u16 sx;
    /* 0x0E */ u16 sy;
} RoomDraw13Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw13Scratch, 0x10);

/// 0x14-byte scratch block `Room_Draw05` takes from `G_SCRATCH_HEAD`. Same
/// projection as `RoomDraw13Scratch` (`arg0` through `Gfx_ViewWorldMtx`, one
/// `RTPS`) plus a second radius: `rOuter` is `(s16)arg2 * 64 / otz` and
/// `rInner` is `(s16)arg2 * 8 / otz`. `flag` is `gte_stflg` and `sx`/`sy` are
/// the projected centre.
typedef struct _RoomDraw05Scratch {
    /* 0x00 */ s32 otz;
    /* 0x04 */ s32 flag;
    /* 0x08 */ s32 rOuter;
    /* 0x0C */ s32 rInner;
    /* 0x10 */ u16 sx;
    /* 0x12 */ u16 sy;
} RoomDraw05Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw05Scratch, 0x14);

/// 0x10-byte scratch block `Room_Draw31` takes from `G_SCRATCH_HEAD`. Same
/// projection as `RoomDraw13Scratch` (`arg0` through `Gfx_ViewWorldMtx`, one
/// `RTPS`) but `radius` sits at 0x4 and `flag` at 0x8. `radius` is
/// `(s16)arg1 * 64 / otz`, the on-screen half-extent of the four `POLY_G4`
/// wedges.
typedef struct _RoomDraw31Scratch {
    /* 0x00 */ s32 otz;
    /* 0x04 */ s32 radius;
    /* 0x08 */ s32 flag;
    /* 0x0C */ u16 sx;
    /* 0x0E */ u16 sy;
} RoomDraw31Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw31Scratch, 0x10);

/// 0x18-byte scratch block `Room_Draw10` takes from `G_SCRATCH_HEAD`. `vec` is
/// the coordinate's `workm.t[]` truncated to s16 and fed to `gte_ldv0`. `otz`
/// is `gte_stszotz` (then incremented so it can also be used as the divisor),
/// `flag` is `gte_stflg` and `sx` / `sy` are the `gte_stsxy` of the single
/// RTPS. `step` is the per-vertex radius `(arg1 * 64) / otz` swept around the
/// ring by `rsin` / `rcos`.
typedef struct _RoomDraw10Scratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     step;
    /* 0x14 */ s16     sx;
    /* 0x16 */ s16     sy;
} RoomDraw10Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw10Scratch, 0x18);

/// 0x18-byte scratch block `Room_Draw14` takes from `G_SCRATCH_HEAD`. Same
/// projection preamble as `RoomDraw10Scratch` (`vec` through `GsWSMATRIX`,
/// one `RTPS`) with `otz` incremented before it is used as the divisor.
/// `radius` is `(s16)arg2 * 23 / otz`, the on-screen half-extent of the
/// axis-aligned `POLY_FT4`.
typedef struct _RoomDraw14Scratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     radius;
    /* 0x14 */ s16     sx;
    /* 0x16 */ s16     sy;
} RoomDraw14Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw14Scratch, 0x18);

/// 0x18-byte scratch block `Room_Draw11`, `Room_Draw12`, `Room_Draw33` and `Room_Draw34` take
/// from `G_SCRATCH_HEAD`. Two `SVECTOR`s (`arg0` and `arg0 + 1`) are projected
/// through `Gfx_ViewWorldMtx`.
/// `otz0`/`otz1` are the `gte_stszotz` of those points, `r0`/`r1` are
/// `(s16)arg1 * 64 / otz`, and `sx0`/`sy0` plus `sx1`/`sy1` are the two
/// `gte_stsxy` centres.
typedef struct _RoomDraw11Scratch {
    /* 0x00 */ s32 otz0;
    /* 0x04 */ s32 otz1;
    /* 0x08 */ s32 r0;
    /* 0x0C */ s32 r1;
    /* 0x10 */ u16 sx0;
    /* 0x12 */ u16 sy0;
    /* 0x14 */ u16 sx1;
    /* 0x16 */ u16 sy1;
} RoomDraw11Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw11Scratch, 0x18);

/// 0x1C-byte scratch block `Room_Draw08` and `Room_Draw01` take from
/// `G_SCRATCH_HEAD`. Same
/// two-point projection as `RoomDraw11Scratch` -- `arg0` and `arg0 + 1` through
/// `Gfx_ViewWorldMtx` -- but the two `gte_stflg` results share `flag` at 0x8,
/// which pushes the radii to 0xC/0x10 and the projected coordinates to
/// 0x14..0x1A. `r0` is `(s16)arg1 * 64 / otz0` and `r1` the same over `otz1`.
typedef struct _RoomDraw08Scratch {
    /* 0x00 */ s32 otz0;
    /* 0x04 */ s32 otz1;
    /* 0x08 */ s32 flag;
    /* 0x0C */ s32 r0;
    /* 0x10 */ s32 r1;
    /* 0x14 */ u16 sx0;
    /* 0x16 */ u16 sy0;
    /* 0x18 */ u16 sx1;
    /* 0x1A */ u16 sy1;
} RoomDraw08Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw08Scratch, 0x1C);

/// 0x18-byte scratch block `Room_Draw04` takes from `G_SCRATCH_HEAD`. Same
/// projection as `RoomDraw10Scratch` (`vec` through `GsWSMATRIX`, one `RTPS`)
/// but `radius` sits at 0xC and `flag` at 0x10. `radius` is
/// `(s16)arg1 * 64 / (otz + 1)`, the on-screen half-extent of the eight
/// `POLY_G4` wedges.
typedef struct _RoomDraw04Scratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     radius;
    /* 0x10 */ s32     flag;
    /* 0x14 */ s16     sx;
    /* 0x16 */ s16     sy;
} RoomDraw04Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw04Scratch, 0x18);

/// 0x1C-byte scratch block `Room_Draw09` takes from `G_SCRATCH_HEAD`. Same
/// projection as `RoomDraw04Scratch` (`vec` through `GsWSMATRIX`, one `RTPS`)
/// plus a second radius: `rOuter` is `(s16)arg1 * 64 / (otz + 1)` and `rInner`
/// is `(s16)(arg1 + arg2) * 64 / (otz + 1)`. `flag` is `gte_stflg` and
/// `sx`/`sy` are the projected centre.
typedef struct _RoomDraw09Scratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     rOuter;
    /* 0x10 */ s32     rInner;
    /* 0x14 */ s32     flag;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} RoomDraw09Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw09Scratch, 0x1C);

/// 0x1C-byte scratch block `Room_DrawBillboard` takes from `G_SCRATCH_HEAD`
/// (and the motel-balcony copies of that body). Same layout as
/// `RoomDraw09Scratch`. `vec` is the world position copied out of the
/// caller's `GsCOORDINATE2` (`workm.t`), projected with a single `RTPS`:
/// `sx`/`sy` come from `gte_stsxy`, `flag` from `gte_stflg` and `otz` from
/// `gte_stszotz`. `otz` is then incremented and used as the divisor that
/// turns the caller's half-size into the two screen-space radii the fan is
/// drawn with - `rOuter` for the eight outer `POLY_G4` wedges, `rInner` for
/// the two inner ones.
typedef RoomDraw09Scratch RoomBillboardScratch;

/// 0x1C-byte scratch block `Room_Draw27` takes from `G_SCRATCH_HEAD`. Same
/// projection preamble as `RoomDraw10Scratch` (`vec` through `GsWSMATRIX`,
/// one `RTPS`) but `dx` / `dy` hold the current
/// `(arg2 * 31 / otz) * rsin|rcos(angle) >> 12` half-extents added to and
/// subtracted from `sx` / `sy` to build the four `POLY_FT4` corners. Only
/// the low halves of `dx` / `dy` are read back. Same layout as the gameplay
/// `GpFxQuadScratch`.
typedef struct _RoomDraw27Scratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} RoomDraw27Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw27Scratch, 0x1C);

/// 0x1C-byte scratch block `Room_Draw19` takes from `G_SCRATCH_HEAD`. Same
/// layout as `RoomDraw27Scratch`.
typedef RoomDraw27Scratch RoomDraw19Scratch;

/// 0x1C-byte scratch block `Room_Draw07` takes from `G_SCRATCH_HEAD`. Same
/// projection and two-radius ring as `RoomDraw09Scratch`, but `flag` sits at
/// 0xC with `rOuter` at 0x10 and `rInner` at 0x14. `rOuter` is
/// `(s16)arg1 * 64 / (otz + 1)` and `rInner` is
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`.
typedef struct _RoomDraw07Scratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     rOuter;
    /* 0x14 */ s32     rInner;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} RoomDraw07Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw07Scratch, 0x1C);

/// 0x1C-byte scratch block `Room_Draw02` takes from `G_SCRATCH_HEAD`. Same
/// projection and two-radius ring as `RoomDraw09Scratch`, but `otz` sits at
/// 0x0 with `rOuter` at 0x4, `rInner` at 0x8, `flag` at 0xC and `vec` at 0x10.
/// `rOuter` is `(s16)arg1 * 64 / (otz + 1)` and `rInner` is
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`.
typedef struct _RoomDraw02Scratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     rOuter;
    /* 0x08 */ s32     rInner;
    /* 0x0C */ s32     flag;
    /* 0x10 */ SVECTOR vec;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} RoomDraw02Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw02Scratch, 0x1C);

/// 0x3C-byte scratch block `Room_Draw03` takes from `G_SCRATCH_HEAD`. `v` is
/// the quad's four corners, copied from `workm.t` of two adjacent slots on
/// each trail. `flag` is `gte_stflg` (negative rejects the quad) and `otz` is
/// `gte_stszotz` (then incremented). `sx0`..`sy3` are the `gte_stsxy` /
/// `gte_stsxy3` of the four corners, copied onto the `POLY_G4` after it is
/// allocated.
typedef struct _RoomDraw03Scratch {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ s32     unused;
    /* 0x2C */ u16     sx0;
    /* 0x2E */ u16     sy0;
    /* 0x30 */ u16     sx1;
    /* 0x32 */ u16     sy1;
    /* 0x34 */ u16     sx2;
    /* 0x36 */ u16     sy2;
    /* 0x38 */ u16     sx3;
    /* 0x3A */ u16     sy3;
} RoomDraw03Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw03Scratch, 0x3C);

#endif // ROOMS_ROOM_COMMON_H
