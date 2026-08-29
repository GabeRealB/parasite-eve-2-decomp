#ifndef GAMEPLAY_4CC_H
#define GAMEPLAY_4CC_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/mc.h"
#include "main/task.h"
#include "main/ui.h"

/// Object at `Task::spawnArg2` for `Gp_BindItemObj2` / `Gp_PublishItemObj` /
/// `Gp_PickupResultTask` / `Gp_WaitItemFlag2`. `field_8` is the packed item id passed
/// to `Gp_GetCurBit2Flag` (and inlined by `Gp_WaitItemFlag2`).
/// `field_A` is the item/location halfword copied into `Gp_PubItemLoc` by
/// `Gp_PublishItemObj` and cleared by `Gp_PickupResultTask` on the cancel path.
typedef struct _GpItemObj8 {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u8   field_8;
    /* 0x09 */ byte pad_9;
    /* 0x0A */ u16  field_A;
} GpItemObj8;

/// Object stored in `Task::extraState` by `Gp_BindItemObj2`. `field_2` is set
/// when `Gp_GetCurBit2Flag` returns 2.
typedef struct _GpItemObj2 {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ u8   field_2;
} GpItemObj2;

/// 8-byte item descriptor in `Gp_ItemDescs` / `Gp_ItemDescsHi`, indexed by item id.
/// `Gp_InitItemSeenBits` / `Gp_GetItemText` use `Gp_ItemDescs[id]` when `id < 0x100`
/// and `Gp_ItemDescsHi[id]` otherwise (same raw index, different base).
/// `field_3` bit 0 gates the `arg1 == 1` result in `Gp_ItemUseRestricted`.
/// `field_4` is a name/string pointer walked by `Gp_GetItemText` /
/// `Gp_InitItemSeenBits` (fields separated by NUL or `'\n'`).
typedef struct _GpItemDesc {
    /* 0x00 */ u8    field_0;
    /* 0x01 */ u8    field_1;
    /* 0x02 */ u8    field_2;
    /* 0x03 */ u8    field_3;
    /* 0x04 */ void* field_4;
} GpItemDesc;
STATIC_ASSERT_SIZEOF(GpItemDesc, 0x8);

/// 4 prompt strings copied onto the stack by `Gp_ItemMenuPrompt` and indexed
/// by `DialogPrompt::field_8`: All / Select / Discard / End
/// (`Gp_StrAll` / `Gp_StrSelect` / `Gp_StrDiscard` / `Gp_StrEnd`).
typedef struct {
    u8* texts[4];
} GpPromptTexts;
STATIC_ASSERT_SIZEOF(GpPromptTexts, 0x10);

/// 0x1C work block allocated by `Gp_ItemMoveTask` (`Mem_Calloc(0x1C, 0)`)
/// and stored at `Task::idMap` / `Gp_ItemMoveWork`. `objs` holds the first two
/// `Ui_SpawnFromDesc` results (the source / dest inventory panes); `field_8` is
/// the index of the pane that currently has focus and is used to index `objs`
/// (`Gp_ItemMoveChild` toggles it with `^ 1`).
typedef struct _GpItemMoveState {
    /* 0x00 */ UiObject* objs[2];
    /* 0x08 */ s32       field_8;
    /* 0x0C */ s32       field_C;
    /* 0x10 */ s32       field_10;
    /* 0x14 */ s32       field_14;
    /* 0x18 */ s32       field_18;
} GpItemMoveState;
STATIC_ASSERT_SIZEOF(GpItemMoveState, 0x1C);

extern GpItemDesc Gp_ItemDescs[];
/// Second item-descriptor base. Indexed as `Gp_ItemDescsHi[id]` for `id >= 0x100`.
extern GpItemDesc Gp_ItemDescsHi[];
/// Packed item/location halfword. High byte selects a UI path in
/// `Gp_SpawnPickupUiTask` (0/1, 8, or default); low byte is written to
/// `Mc_SaveData.field_12` on the case-8 path. Copied into `Gp_MoveItemKey`
/// by `Gp_ItemMoveTask`.
extern u16 Gp_PubItemLoc;
/// Extra halfword copied from `GpBit2Rec.field_6` by `Gp_LookupBit2Item`
/// (always written on a match; delay slot of the item-id `< 0x100` check).
extern u16 D_80114DDE;
/// Set to 1 by `Gp_PublishItemObj` after it publishes `Gp_PubItemLoc` /
/// `Gp_PubItemQty`.
extern u16 Gp_PubItemReady;
/// Stack/count halfword published by `Gp_PublishItemObj`: 1 for item ids
/// below 0xA0, else `Gp_StackLimits[id - 0xA0].field_0`.
extern u16 Gp_PubItemQty;
/// Packed item id copied from `GpItemObj8.field_8` by `Gp_PublishItemObj`.
extern u16 Gp_PubItemId;
/// Item/location halfword copied from `Gp_PubItemLoc` by `Gp_ItemMoveTask`.
/// `Gp_ItemUseRestricted` special-cases the value `0x703`.
extern u16 Gp_MoveItemKey;
/// Item-move UI work block published by `Gp_ItemMoveTask`.
extern GpItemMoveState* Gp_ItemMoveWork;
/// Item-move `UiObjectDesc` table. `Gp_ItemMoveTask` spawns `[0]` / `[1]`
/// and, when `spawnArg1 == 1`, `[9]`.
extern UiObjectDesc D_8010D6F4[];
/// Popup spawned by `Gp_ItemMoveRow` on confirm when `owner->state == 1`.
extern UiObjectDesc D_8010D764;
/// "Move items" confirmation popup spawned by `Gp_ItemMoveChild` when the
/// pane is closed with items still selected (`Gp_CanMoveItems` result as arg1).
extern UiObjectDesc D_8010D7F0;
/// Extra `UiObjectDesc` spawned after the `D_8010D6F4` pair.
extern UiObjectDesc D_8010D80C;
/// Pair of inventory UiLists indexed by `Task::spawnArg1` (source / dest).
/// `field_10` is the selected row passed to `Gp_GetScanSlot`.
extern UiList Gp_InvLists[];
/// Action-button callbacks for `Gp_ItemActionList`, filled by `Gp_FillItemActions`.
extern UiListItemFunc Gp_ItemActionFns[];
/// UiList used by `Gp_ItemActionListTask`.
extern UiList Gp_ItemActionList;
/// UiList used by `Gp_ItemMenuListTask`. `field_10` is 1 when `spawnArg1` is 0.
extern UiList              Gp_ItemMenuList;
extern const char          Gp_StrBattleField[]; // "Battle Field"
extern const char          Gp_StrItemBox[];     // "Item Box"
extern const char          Gp_StrPlayerItem[];  // "Player Item"
extern u8                  Gp_StrAll[];         // "All"
extern u8                  Gp_StrSelect[];      // "Select"
extern u8                  Gp_StrDiscard[];     // "Discard"
extern u8                  Gp_StrEnd[];         // "End"
extern char                Gp_StrBullet[];      // "Bullet"
extern const GpPromptTexts Gp_ItemPromptTexts;
/// Fullscreen-fade vector template used by `Gp_FadeTileTask` / `Gp_ItemPickupTilt`.
extern const VECTOR D_80093DB0;
/// Per-child item-move handler. Walked by `Gp_ItemMoveTask` over
/// `obj->owner`'s children as `Gp_ItemMoveChild(child->spawnArg2, child)`.
void Gp_ItemMoveChild(UiObject* arg0, Task* arg1);
/// Task callback for the item-move UI. `spawnArg2` is the `UiObject`.
/// First run copies `Gp_ScanPtrs[Gp_PubItemLoc]` / `Mc_SaveData.field_5BC`
/// into `Gp_MoveScanSrc` / `Gp_MoveScanDst`, spawns the `D_8010D6F4` pair
/// (plus `[9]` when `spawnArg1 == 1`), then walks children through
/// `Gp_ItemMoveChild`. Always writes `field_2C = 0x34`.
void Gp_ItemMoveTask(Task* arg0);
/// List-item callback for an inventory row. Looks up
/// `Gp_MoveScanSrc[owner->spawnArg1]` at `field_8`, highlights the move-source
/// row in `0x37A78`, draws the item (and ammo count for ids `0xA0..0xBF`),
/// then on confirm either opens the stack/info popup (`owner->state == 1`)
/// or starts a move / restriction prompt.
void Gp_ItemMoveRow(DialogPrompt* arg0, UiObject* arg1);
/// Task callback for one `Gp_InvLists` inventory pane. `spawnArg1 >= 0x100`
/// is masked to the low byte and `flags` is set so the title is
/// `Gp_StrBattleField` ("Battle Field") instead of `Gp_StrItemBox` ("Item Box");
/// dest (`spawnArg1 != 0`) uses `Gp_StrPlayerItem` ("Player Item"). Seeds the
/// list from `Gp_MoveScanSrc[spawnArg1].field_1` (visible rows capped at 10).
/// First-state confirm/cancel is `field_2E = -1`; later states write
/// `0x24`. Circle (src) / Square (dest) / mask 3 switch panes (`0xA`)
/// and play type-6 sound 2. Walks children through `Gp_CloseItemPane`.
void Gp_ItemPaneTask(Task* arg0);
void func_800BD6DC(DialogPrompt* arg0, UiObject* arg1);
/// List-item confirm for `Gp_ItemActionFns`. Draws `Gp_StrSwitch`, then on confirm
/// looks up the selected inventory row and inlines `Gp_ItemUseRestricted` against
/// `owner->parent->flags`. A true result opens prompt `0x1E`; dest inventory
/// (`spawnArg1 == 1`) plus an equipped weapon/armor (`field_21+0x7F` /
/// `field_23+0x5F`) opens prompt `7`; otherwise `field_2E = 0x23`.
void Gp_ItemActionConfirm(DialogPrompt* arg0, UiObject* arg1);
/// Fills `Gp_ItemActionFns` and `arg0->field_4` / `field_5` from the selected
/// inventory row (`Gp_MoveScanSrc[spawnArg1]` / `Gp_InvLists[spawnArg1].field_10`).
void Gp_FillItemActions(UiList* arg0, UiObject* arg1);
/// Task callback for the `Gp_ItemActionList` item list. On first run it copies
/// `parent->flags`, clamps `field_E + field_12` to 0x64, then calls
/// `Gp_FillItemActions` and `Ui_LayoutListPanel`. Confirm (`D_8005ED78`) is
/// cancel (`field_2E = -1`) when `owner->flags` is 0, else 6; cancel
/// (`D_8005ED74`) is 6. Child `field_2E` -1 / 9 / 6 closes, remaps to
/// 6, or teardowns.
void Gp_ItemActionListTask(Task* arg0);
/// List-item callback for All / Select / Discard / End. Draws
/// `Gp_ItemPromptTexts[field_8]`. Confirm: All → `field_2E = 0x26`, Select → 6,
/// Discard strips 0x80–0x9F attachments missing from
/// `Mc_SaveData.field_5BC` and sets `field_2E = 0x27`. Cancel once sets
/// `field_10 = 2` / `field_22 = 0x21`; a second cancel does the discard
/// strip.
void Gp_ItemMenuPrompt(DialogPrompt* arg0, UiObject* arg1);
/// Task callback. `extra` is a `GameActorExt`; `spawnArg2` is a `GpItemObj8`.
/// Tilts `field_8[2]` (a `GsCOORDINATE2`) while playing a location-specific
/// type-6 sound, then signals `extraState` (`GpItemObj2.field_2 = 1`) when
/// the motion returns to 0.
void Gp_ItemPickupTilt(Task* arg0);
void Gp_ForEachUiChild(UiObject* arg0, void (*arg1)(UiObject*, Task*));
s32  Gp_ItemUseRestricted(s32 arg0, s32 arg1);
/// Child closer for the `Gp_InvLists` inventory panes. `-1` tears down and
/// either restores parent status or sets parent `field_2E = -1` when the
/// parent owner has no flags; `6` / `0x23` / `38` / `39` copy those codes
/// onto the parent (`6` also restores status).
void Gp_CloseItemPane(UiObject* arg0, Task* arg1);
void Gp_ItemMenuListTask(Task* arg0);
/// Task callback. `spawnArg2` is the `UiObject`; on first run it is published
/// as `Wip_UiHolder`. `spawnArg1` is a text pointer; when non-zero, two prompt
/// lines are drawn at `field_18 + 0xF` / `+ 0x1E` in color `0x606060`.
void Gp_HolderPromptTask(Task* arg0);
s32  Gp_BindItemObj2(Task* arg0, s32 arg1, GpItemObj2* arg2);
/// First state of the `D_80096E70` dispatcher. Copies `field_8` /
/// `field_A` into `Gp_PubItemId` / `Gp_PubItemLoc`, remaps owned 0x60–0x7F
/// items to 0xD and 0x80–0x9F items to 0x3D, then publishes a stack
/// count in `Gp_PubItemQty`.
void Gp_PublishItemObj(Task* arg0);
/// Fullscreen semi-trans TILE fade. `spawnArg1` 0/2 count down from 7/8;
/// 4 also counts down once `Display_State.field_100 == 2`; 5 and other
/// values count up and write `field_100` / `field_103` on completion.
void Gp_FadeTileTask(Task* arg0);

#endif // GAMEPLAY_4CC_H
