#ifndef GAMEPLAY_PRIVATE_ITEMS_H
#define GAMEPLAY_PRIVATE_ITEMS_H

#include "types.h"

#include "gameplay/inventory.h"
#include "inventory.h"

#include "main/mc_types.h"
#include "main/task_types.h"

// Inventory contents, collection flags, quantities, sorting and equipment.

/// Qty table indexed by raw item id. `Gp_RelatedQty1` is the 0x80–0x9F slice
/// at +0x200 (`Gp_EquipRelatedBank`).
extern GpRelatedItemTable Gp_RelatedQty1;

extern GpItemMap Gp_ItemMaps[];

extern InventoryItemRow* Gp_ItemTable1;

extern const char Gp_StrNotice2[8];

s32 Gp_EquipRelatedBank(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

struct UiObject;

s32 Gp_RemoveItem(InventoryItemRange* arg0, InventoryItemRow* arg1, s32 arg2);

/// Confirmation UI for raising `Mc_SaveData[0].state.itemLevelBonus` of the equipped
/// 0x60–0x7F item (`Player_Status.armor`). If the clamped level is
/// already 10, `Gp_NoticePanelTask` is shown with spawnArg1 0x1A. Otherwise
/// consumes `Gp_SelItemRec` and draws "More <item> attachments available."
void Gp_UiBoostAttach(struct UiObject* arg0, Task* arg1);

void Gp_UiBoostMp(struct UiObject* arg0, Task* arg1);

/// HP counterpart of `Gp_UiBoostMp`: adds 5 to `Mc_SaveData[0].state.hpBonus`
/// (clamped below 250), recomputes max HP (same body as `Gp_RecalcMaxHp`),
/// heals current HP to that max, then consumes `Gp_SelItemRec` and spawns
/// `Gp_BoostPanelDesc`. `Gp_NoticePanelTask` is called with `spawnArg1` forced to 0x1C.
void Gp_UiBoostHp(struct UiObject* arg0, Task* arg1);

s32 func_800B9D80(s32 arg0);

void Gp_ApplyBit2Bank(s32 arg0);

s32 Gp_CountCollectedBits(void);

s32 Gp_CountEquippedRelated(InventoryItemRange* arg0, s32 arg1);

void Gp_ClearEquipSlot(s32 arg0);

void Gp_ClearEquipSlotSel(s32 arg0, s32 arg1);

void Gp_ConsumeScanQty(InventoryItemRange* arg0, s32 arg1, s32 arg2);

s32 Gp_FillRelated(s32 arg0, s32 arg1);

s32 Gp_UnequipRelated(s32 arg0, s32 arg1);

s32 Gp_ScanIndexOf(InventoryItemRange* arg0, InventoryItemRow* arg1);

/// `arg2` is unused; some callers pass 0 so the `jal` delay slot is `move a2, zero`.
InventoryItemRow* Gp_GetScanSlot(InventoryItemRange* arg0, s32 arg1, s32 arg2);

void Gp_InitModeEquip(void);

void Gp_ClearCollectedBits(void);

/// `arg1` is unused; some callers pass 0 so the `jal` delay slot is `move a1, zero`.
s32 Gp_NthCollectedId(s32 arg0, s32 arg1);

/// Angle units used when capturing the saved player facing (4096 per turn).
enum {
    PLAYER_YAW_HALF_TURN = 0x800,
    PLAYER_YAW_FULL_TURN = 0x1000
};

void Gp_SavePlayerPos(void);

void Gp_SyncHeldRelated(void);

s32 Gp_HasItemSeenBit(s32 arg0);

/// Number of rows `scan` covers, i.e. how many items the window can hold.
s32 Gp_GetScanCount(InventoryItemRange* scan);

s32 Gp_GetModLevel(s32 arg0);

void Gp_TickBoostPanel(Task* arg0);

s32 Gp_FindScanQty(InventoryItemRow* arg0, InventoryItemRange* arg1, s32* arg2, s32 arg3);

void Gp_AgeFlag119Void(void);

void func_800B8014(void);

/// Moves the item at scan slot `arg1` onto slot `arg2`, shifting the
/// occupied rows between them toward the hole left at `arg1`.
void Gp_MoveItemSlot(InventoryItemRange* arg0, s32 arg1, s32 arg2);

extern GpItemAttr Gp_ModStatAttrs[];

// Shared item-publication work.
extern u16 Gp_PubItemReady;

extern u32 D_80114DCC;

extern u16 Gp_PubItemQty;

extern InventoryItemRow* Gp_SelItemRec;

extern s32 D_80114DD8;

extern u16 Gp_PubItemLoc;

extern u16 D_80114DDE;

extern s32 D_80114DE0;

extern s32 D_80114DE4;

extern s32 D_80114DE8;

extern u16 Gp_PubItemId;

s32 Gp_LookupBit2Item(s32 arg0);

/// Byte remap of an item id used as a sort/order key (`Gp_ItemSortKey` /
/// `Gp_SortItems`). Split by item class: 0x01–0x5F → `Gp_ItemSortKey0[id]`,
/// 0x60–0x7F → `Gp_ItemSortKey60[id-0x60]`, 0x80–0x9F → `Gp_ItemSortKey80[id-0x80]`,
/// 0xA0–0xBF → `Gp_ItemSortKeyA0[id-0xA0]`. A 0 entry (or an id outside those
/// ranges) falls back to `id + 0x100`; id 0 returns 0x1000.
extern u8 Gp_ItemSortKey0[];

extern u8 Gp_ItemSortKey60[];

extern u8 Gp_ItemSortKey80[];

extern u8 Gp_ItemSortKeyA0[];

/// Selection-sorts the item table selected by `arg0` using the same
/// sort-key remap as `Gp_ItemSortKey` (`Gp_ItemSortKey0` / `Gp_ItemSortKey60` /
/// `Gp_ItemSortKey80` / `Gp_ItemSortKeyA0`). `arg1` is unused.
void Gp_SortItems(InventoryItemRange* arg0, s32 arg1);

/// Writes item `arg2` into scan slot `arg1`. Ids `0xA0..0xBF` are added with
/// `Gp_GiveItem` first, then an existing stack is moved onto the slot when
/// it is empty. Other ids overwrite the slot (re-adding the previous item).
InventoryItemRow* Gp_SetScanItem(InventoryItemRange* arg0, s32 arg1, s32 arg2, s32 arg3);

/// Returns the `arg1`-th matching item id from the table selected by `arg0`.
/// `0x80..0x9F` ids match when `arg2 == 0`, or when `arg2` is a related id
/// in `Gp_RelatedQty0` / `Gp_RelatedQty1` and the row is stocked or selected.
s32 Gp_NthRelatedId(InventoryItemRange* arg0, s32 arg1, s32 arg2);

void Gp_RefreshItemRow(InventoryItemRow* arg0);

/// Adds `arg2` of item `arg1` to the item table selected by `arg0`.
/// Ids `0xA0..0xBF` stack onto an existing row, clamped to
/// `Gp_StackLimits[id-0xA0].maxHeld`. `arg2 < 0` uses that row's `field_0`
/// as the count, or `field_2` when `arg2 == -2`; out-of-range ids use 1.
/// Other ids take the first free slot with quantity 1. Returns the
/// written row, or NULL if none was free.
InventoryItemRow* Gp_AddItem(InventoryItemRange* arg0, s32 arg1, s32 arg2);

void func_800B92CC(Task* task);

extern InventoryItemRange Gp_DefaultScan;

#endif // GAMEPLAY_PRIVATE_ITEMS_H
