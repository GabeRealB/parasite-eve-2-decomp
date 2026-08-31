#ifndef GAMEPLAY_3688_H
#define GAMEPLAY_3688_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/ui.h"

struct _UiObject;

/// Callback for UiObject + Task state handlers (e.g. entries in `Gp_ItemMenuStates`).
typedef void (*UiObjectTaskFunc)(struct _UiObject* arg0, Task* arg1);

/// Fixed-size table of `UiObjectTaskFunc` callbacks. Copied onto the stack by
/// `Gp_ItemMenuTask` so the call uses a local jump table.
typedef struct {
    UiObjectTaskFunc funcs[3];
} UiObjectTaskFuncTable3;

/// Five-entry dispatcher table: `Gp_PublishItemObj`, `Gp_SpawnPickupUiTask`, `Gp_PickupResultTask`,
/// `func_800CE188`, `Gp_PickupExitTask`. Copied onto the stack by `func_800CE22C`.
extern TaskFuncTable5 D_80096E70;

/// Three-entry dispatcher table: `Gp_ItemMenuInit`, `Gp_UiPromptUpdate`, `Gp_UiPromptDispatch`.
extern const UiObjectTaskFuncTable3 Gp_ItemMenuStates;

/// CLUT ids for the ten item-category icons drawn by `Gp_DrawItemIcon`,
/// indexed by the icon index that function derives from the item id.
extern const u16 D_80096F88[];

/// Four-entry dispatcher table: `Gp_MapPanelInit`, `Gp_MapFirstDrawTask`, `Gp_MapTaskState2`,
/// `Gp_MapDrawTask`.
extern TaskFuncTable4 Gp_MapTaskStates;

/// 0xE-byte per-room record in tables pointed to by `Gp_MapRecTables`.
/// Indexed by `GameSession.field_7 - 1` then `GameSession.field_6`.
/// field_0/field_2 are signed coords, field_4/field_6 unsigned extents,
/// field_8/field_A signed scales (`Gp_DrawMapCursor`); field_C is the
/// room id stored in `Gp_MapRoomId` (`Gp_GetMapRoomId`).
typedef struct _GpMapRec {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ u16 field_4;
    /* 0x06 */ u16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u16 field_C;
} GpMapRec;
STATIC_ASSERT_SIZEOF(GpMapRec, 0xE);

/// Per-stage table of `GpMapRec` arrays. Index is `GameSession.field_7 - 1`.
extern GpMapRec* Gp_MapRecTables[];

/// 0x20-byte per-room name string in tables pointed to by `Gp_MapNameTables`.
/// Indexed by `GameSession.field_7 - 1` then `GameSession.field_6 - 1`.
typedef struct _GpMapName {
    /* 0x00 */ u8 text[0x20];
} GpMapName;
STATIC_ASSERT_SIZEOF(GpMapName, 0x20);

/// Per-stage table of `GpMapName` arrays. Index is `GameSession.field_7 - 1`.
/// A NULL entry skips the name draw (`Gp_DrawMapName`).
extern GpMapName* Gp_MapNameTables[];

/// 8-byte map marker in tables pointed to by `Gp_MapMarkTables`.
/// Indexed by loop `i` in `Gp_DrawMapMarks`. `field_0` is the icon object
/// (`func_800D4270`); `field_4` is the room id (`Gp_MapRoomId`); `field_5`
/// is an extra bit id (`0xFF` = none).
typedef struct _GpMapMark {
    /* 0x0 */ void* field_0;
    /* 0x4 */ u8    field_4;
    /* 0x5 */ u8    field_5;
    /* 0x6 */ byte  pad_6[2];
} GpMapMark;
STATIC_ASSERT_SIZEOF(GpMapMark, 8);

/// Per-stage table of `GpMapMark` arrays. Index is `GameSession.field_7 - 1`.
extern GpMapMark* Gp_MapMarkTables[];

/// 8-byte map icon record in the per-stage tables pointed to by `D_8010F0CC`.
/// Walked by `Gp_DrawMapIcons` until `field_0` is 0. `field_0` is the room id
/// (`Gp_MapRoomId`), `field_1` the `Gp_DrawMapMarks` marker index the icon
/// belongs to, `field_2` the icon kind (0 / 1 / 2; kind 2 is the blinking
/// "current objective" icon gated on `func_800E3FCC(0xA2)`), `field_3` a
/// GameFlag nibble id (0 = always shown); `x` / `y` are the map coordinates.
typedef struct _GpMapIcon {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u16 x;
    /* 0x6 */ u16 y;
} GpMapIcon;
STATIC_ASSERT_SIZEOF(GpMapIcon, 8);

/// Per-stage table of `GpMapIcon` arrays. Index is `GameSession.field_7 - 1`.
extern GpMapIcon* D_8010F0CC[];

/// 0xC-byte scratchpad block `Gp_DrawMapIcons` carves off `G_SCRATCH_HEAD` to
/// stage one map icon position before it is turned into a `SPRT_16`.
typedef struct _GpMapIconPos {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ u16 field_8;
    /* 0xA */ u16 field_A;
} GpMapIconPos;
STATIC_ASSERT_SIZEOF(GpMapIconPos, 0xC);

/// 0x1C-byte scratch block `Gp_DrawMapCursor` / `func_800D0614` carve off
/// `G_SCRATCH_HEAD` to stage the player cursor position on the map screen.
/// `x` / `y` are the map coordinates; only the tail from 0xC on is written.
typedef struct _GpMapCursorPos {
    /* 0x00 */ byte pad_0[0xC];
    /* 0x0C */ u16  x;
    /* 0x0E */ u16  y;
    /* 0x10 */ u16  field_10;
    /* 0x12 */ u16  field_12;
    /* 0x14 */ u16  field_14;
    /* 0x16 */ byte pad_16[6];
} GpMapCursorPos;
STATIC_ASSERT_SIZEOF(GpMapCursorPos, 0x1C);

/// Per-stage table of GameFlag nibble ids, indexed by room (`Gp_MapRoomId`).
/// Index is `GameSession.field_7 - 1`.
extern u8* Gp_MapFlagIds[];

/// Per-stage `GpMapMark` counts. Index is `GameSession.field_7 - 1`.
extern u8 Gp_MapMarkCounts[];

/// Current room id copied from `GpMapRec.field_C` by `Gp_GetMapRoomId`.
extern u8 Gp_MapRoomId;

/// Room-id offset applied by `Gp_EnqueueMapRoomCd` (0, or 1 / 3 for two flagged rooms).
extern u8 Gp_MapRoomOff;

/// Per-column icon descriptor for the P.Energy attach panel (`Gp_PeGridPanelTask`).
/// One 3-byte entry per weapon/armour column: `u`/`v` are the SPRT texture
/// coordinates of the column caption and `xOffset` shifts the caption right of
/// the column origin. The table `D_8010E844` holds the four columns.
typedef struct _GpEnergyIcon {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 xOffset;
} GpEnergyIcon;
STATIC_ASSERT_SIZEOF(GpEnergyIcon, 3);

/// Column caption table drawn across the top of the P.Energy attach panel.
extern GpEnergyIcon D_8010E844[4];

void Gp_DrawItemLabel(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
void Gp_DrawQty(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void Gp_SetPreviewItem(s32 arg0, s32 arg1);
void Gp_SetHolderItemText(s32 arg0);
void Gp_DrawUsePrompt(DialogPrompt* arg0, UiObject* arg1);
void Gp_SpawnItemPrompt(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3);
u8   Gp_GetMapRoomId(void);
s32  func_800D4D2C(s32 arg0);

#endif // GAMEPLAY_3688_H
