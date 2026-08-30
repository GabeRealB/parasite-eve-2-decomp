#ifndef GAMEPLAY_268_H
#define GAMEPLAY_268_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

struct _GpEnemy;
struct _UiObject;

/// Save-inventory slot (`Mc_SaveData.field_1C8`). field_0/field_2 are item ids;
/// field_1/field_3 are the matching counts (`Gp_CountEquippedRelated`).
typedef McItemSlot GpItemSlot;

/// 4-byte table entry in `Gp_ItemMaps` (8 entries). field_1 is an item id
/// used to index `Mc_SaveData.field_1C8`; field_0 selects which (id, count) pair;
/// field_2 is the mapped item id (`Gp_ApplyItemMap`).
typedef struct _GpItemMap {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
} GpItemMap;
STATIC_ASSERT_SIZEOF(GpItemMap, 0x4);

/// 4-byte scan descriptor for `Gp_GetItemTable` / `Gp_SumScanQty` /
/// `Gp_GetScanSlot` / `Gp_ScanIndexOf` / `Gp_GetScanItemId` / `Gp_HasStockedItem` /
/// `Gp_CountScanItems` / `Gp_CanAddItem`.
/// field_0 is the start index into a 4-byte item table; field_1 is the
/// entry count; field_2 selects the table (1 / 2 / default). Also
/// `Mc_SaveData.field_5BC`. `Gp_DefaultScan` is a ROM default copied there
/// by `Gp_ResetScanDefault`.
typedef McItemScan GpItemScan;

/// 4-byte row in the item tables selected by `GpItemScan`
/// (`Mc_SaveData.field_1AC` / `Gp_ItemTable2` / `*Gp_ItemTable1`). field_0 is the
/// item id looked up by `Gp_FindItemById` / `Gp_FindItemInScan` / returned by
/// `Gp_GetScanItemId` / matched by `Gp_HasStockedItem`; field_1 is a count
/// compared as signed by `Gp_EquipHeld` / `Gp_RefreshItemRow` /
/// `Gp_FindItemByKind` (`== arg0 + 1`) /
/// `Gp_HasStockedItem` (`> 0`); field_2 is a u16 quantity added by
/// `Gp_SumScanQty`.
typedef McItemRec GpItemRec;

/// 4-byte entry in `Gp_RelatedQty1` / `Gp_RelatedQty0` (32 entries, item ids
/// 0x80–0x9F). field_0 is a count (`Gp_GetRelatedQty` / `Gp_ApplyItemMap`);
/// field_1–3 are related item ids (`Gp_BuildAttachList` / `Gp_NthRelatedId` /
/// `Gp_NthStockRelated` / `Gp_EquipRelatedBank`).
/// `Gp_RelatedQty0` is the first `GpItemSlot` pair (arg0 == 0);
/// `Gp_RelatedQty1` is the second.
/// `Gp_QtyById0` / `Gp_QtyById1` are the same tables indexed by raw item id
/// (`Gp_RelatedQty0` is `Gp_QtyById0 + 0x200`).
typedef struct _GpItemQty {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
} GpItemQty;
STATIC_ASSERT_SIZEOF(GpItemQty, 0x4);

/// 16-byte record walked by `Gp_ApplyBit2List` / `Gp_ApplyBit2Bank` /
/// `Gp_LookupBit2Item`. field_0 is a packed item id (0xFFFF terminator);
/// field_2 is the item id published to `Gp_PubItemLoc` by `Gp_LookupBit2Item`;
/// field_6 is the extra halfword published to `D_80114DDE` (and the low
/// 2 bits written into the dest 2-bit bank by `Gp_ApplyBit2List`).
typedef struct _GpBit2Rec {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ u16  field_2;
    /* 0x04 */ byte pad_4[2];
    /* 0x06 */ u16  field_6;
    /* 0x08 */ byte pad_8[8];
} GpBit2Rec;
STATIC_ASSERT_SIZEOF(GpBit2Rec, 0x10);

/// Spawn header for `Gp_SpawnAtPlace` / `Gp_SpawnPlaces`. `field_0` is
/// `Task_SpawnFromTable` arg2 (0xFFFF terminator); `field_4` is the
/// `TaskDesc` table (`Gp_SpawnEnemyFromTable` uses idx 0).
typedef struct _GpEnemyDesc {
    /* 0x0 */ u16      field_0;
    /* 0x2 */ byte     pad_2[2];
    /* 0x4 */ TaskDesc field_4;
} GpEnemyDesc;
STATIC_ASSERT_SIZEOF(GpEnemyDesc, 0x10);

/// 8-byte list node walked by `Gp_ApplyBit2List` / `Gp_ApplyBit2Bank` /
/// `Gp_LookupBit2Item`. field_0 is a `GpBit2Rec` list (NULL skips;
/// `(GpBit2Rec*)-1` ends in `Gp_ApplyBit2List` / `Gp_ApplyBit2Bank`;
/// `(GpBit2Rec*)0x7FFFFFFF` ends in `Gp_LookupBit2Item`).
/// `Gp_SpawnPlaces` / `Gp_SpawnPlaceById` also read field_0 as a `GpEnemyPlace`
/// list and field_4 as a 0xFFFF-terminated `GpEnemyDesc` table.
/// `Gp_Bit2Banks[i].field_0` points at a table of these.
typedef struct _GpBit2List {
    /* 0x00 */ GpBit2Rec*   field_0;
    /* 0x04 */ GpEnemyDesc* field_4;
} GpBit2List;
STATIC_ASSERT_SIZEOF(GpBit2List, 0x8);

/// 8-byte entry in `Gp_Bit2Banks`, indexed by session field_7 /
/// `GameSessionFrom4.field_3` / `Mc_SaveData.field_7`. field_0 is a
/// `GpBit2List` table applied by `Gp_ApplyBit2Bank` / `Gp_ApplyBit2List`.
/// field_4 is packed 2-bit flags (`Gp_GetBit2Flag` / `Gp_SetBit2Flag` /
/// `Gp_GetCurBit2Flag` / `Gp_SetCurBit2Flag` / `Gp_SpawnPlaceById`).
typedef struct _GpBit2Bank {
    /* 0x00 */ GpBit2List* field_0;
    /* 0x04 */ u32*        field_4;
} GpBit2Bank;
STATIC_ASSERT_SIZEOF(GpBit2Bank, 0x8);

/// Placement record for `Gp_SpawnAtPlace` / `Gp_SpawnPlaces` / `Gp_SpawnPlaceById`. `field_0` / `field_4` pack into
/// `GpEnemy.field_8` as `field_0 | (field_4 << 8)`; `field_2` is copied to
/// `GpEnemy.field_A`. `field_8` / `field_A` / `field_C` are world X/Y/Z
/// (`GsCOORDINATE2.coord.t`); `field_E` is the yaw stored at coord +0x46
/// and passed to `Gfx_RotMatrixY` when non-zero.
typedef struct _GpEnemyPlace {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u16  field_2;
    /* 0x4 */ u16  field_4;
    /* 0x6 */ byte pad_6[2];
    /* 0x8 */ s16  field_8;
    /* 0xA */ s16  field_A;
    /* 0xC */ s16  field_C;
    /* 0xE */ u16  field_E;
} GpEnemyPlace;
STATIC_ASSERT_SIZEOF(GpEnemyPlace, 0x10);

/// Overlay of `GsCOORDINATE2` at `GameActorExt.field_8` used by `Gp_SpawnAtPlace`.
/// `coord` is `GsCOORDINATE2.coord` (`Gfx_RotMatrixY` / `t[0..2]`); `field_46`
/// is the yaw halfword next to `param`.
typedef struct _GpCoordPlace {
    /* 0x00 */ s32    flg;
    /* 0x04 */ MATRIX coord;
    /* 0x24 */ byte   pad_24[0x22];
    /* 0x46 */ s16    field_46;
} GpCoordPlace;
STATIC_ASSERT_SIZEOF(GpCoordPlace, 0x48);

/// Overlay of `GsCOORDINATE2` at `GameActorExt.field_8` used by `Gp_SavePlayerPos`.
/// field_8 / field_14 are `coord.m[0][2]` / `coord.m[2][2]` (`lh` into
/// `ratan2`); field_18 / field_1C / field_20 are the low 16 bits of
/// `coord.t[0..2]` (`lhu`).
typedef struct _GpCoordYaw {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ s16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ s16  field_14;
    /* 0x16 */ byte pad_16[2];
    /* 0x18 */ u16  field_18;
    /* 0x1A */ byte pad_1A[2];
    /* 0x1C */ u16  field_1C;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ u16  field_20;
} GpCoordYaw;
STATIC_ASSERT_SIZEOF(GpCoordYaw, 0x22);

/// 8-byte item attribute row. `Gp_ItemAttrs` is indexed by raw item id
/// (`Gp_GetModLevel`); ids 0x60–0x7F land in the `Gp_ModStatAttrs` slice.
/// field_4 is the unsigned bonus added to `Wip_SysConfig.field_1a` by
/// `Gp_RecalcMaxHp` when `Wip_SysConfig.field_23` (item id − 0x5F) is
/// non-zero. field_5 is the unsigned base added to
/// `Mc_SaveData.field_908[id-0x60]` and clamped to 10. field_6 is the
/// unsigned bonus added to `Wip_SysConfig.field_1e` by `Gp_RecalcMaxMp`
/// when `field_23` is non-zero.
typedef struct _GpItemAttr {
    /* 0x00 */ u8 field_0;
    /* 0x01 */ u8 field_1;
    /* 0x02 */ u8 field_2;
    /* 0x03 */ u8 field_3;
    /* 0x04 */ u8 field_4;
    /* 0x05 */ u8 field_5;
    /* 0x06 */ u8 field_6;
    /* 0x07 */ u8 field_7;
} GpItemAttr;
STATIC_ASSERT_SIZEOF(GpItemAttr, 0x8);

/// 4-byte row in `Gp_StackLimits`, indexed by item id − 0xA0 (ids ≥ 0xA0).
/// field_0 is the default count copied to `Gp_PubItemQty` by `Gp_PublishItemObj`.
/// field_2 is a max quantity (`Gp_AddItem` / `Gp_LookupBit2Item`).
typedef struct _GpItemA0 {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u16 field_2;
} GpItemA0;
STATIC_ASSERT_SIZEOF(GpItemA0, 0x4);

/// 8-byte row in `Gp_StatRows` (4 entries), indexed by `Mc_SaveData.field_F`.
/// field_0 is the unsigned base written into `Wip_SysConfig.field_1a`
/// (`Gp_RecalcMaxHp`). field_4 is the word added into `Wip_SysConfig.field_1e`
/// (`Gp_RecalcMaxMp`).
typedef struct _GpStatRow {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ byte pad_2[2];
    /* 0x04 */ s32  field_4;
} GpStatRow;
STATIC_ASSERT_SIZEOF(GpStatRow, 0x8);

extern GpBit2Bank Gp_Bit2Banks[];
/// Qty table indexed by raw item id. `Gp_RelatedQty1` is the 0x80–0x9F slice
/// at +0x200 (`Gp_EquipRelatedBank` / `Gp_EquipRelatedItem`).
extern GpItemQty  Gp_QtyById1[];
extern GpItemQty  Gp_RelatedQty1[];
extern GpItemMap  Gp_ItemMaps[];
extern GpItemScan Gp_DefaultScan;
/// Source item-table scan (`Gp_CanMoveItems` / item-move UI). field_0 is the
/// start index, field_1 the entry count, field_2 the table id.
extern GpItemScan Gp_MoveScanSrc;
/// Dest item-table scan immediately after `Gp_MoveScanSrc` (`Gp_CanMoveItems`).
extern GpItemScan Gp_MoveScanDst;
/// Array of `GpItemScan*` (`Gp_InitStarterInv` clears `[1]` and `[2]`).
extern GpItemScan* Gp_ScanPtrs[];
/// Scan dest used while `Gp_InitStarterInv` copies the current inventory out.
extern GpItemScan* D_8010D55C;
extern GpStatRow   Gp_StatRows[];
extern GpItemAttr  Gp_ItemAttrs[];
/// Qty table indexed by raw item id. `Gp_RelatedQty0` is the 0x80–0x9F slice
/// at +0x200 (`Gp_EquipRelatedBank` / `Gp_EquipRelatedItem`).
extern GpItemQty  Gp_QtyById0[];
extern GpItemQty  Gp_RelatedQty0[];
extern GpItemAttr Gp_ModStatAttrs[];
extern GpItemA0   Gp_StackLimits[];
/// Byte remap of an item id used as a sort/order key (`Gp_ItemSortKey` /
/// `Gp_SortItems`). Split by item class: 0x01–0x5F → `Gp_ItemSortKey0[id]`,
/// 0x60–0x7F → `Gp_ItemSortKey60[id-0x60]`, 0x80–0x9F → `Gp_ItemSortKey80[id-0x80]`,
/// 0xA0–0xBF → `Gp_ItemSortKeyA0[id-0xA0]`. A 0 entry (or an id outside those
/// ranges) falls back to `id + 0x100`; id 0 returns 0x1000.
extern u8 Gp_ItemSortKey0[];
extern u8 Gp_ItemSortKey60[];
extern u8 Gp_ItemSortKey80[];
extern u8 Gp_ItemSortKeyA0[];
/// 0xFFFF-terminated item-id list walked by `Gp_NthCollectedId`. Each id's
/// low 7 bits index a collected-item bit in `Mc_SaveData.field_5AC`.
extern u16        Gp_CollectedIds[];
extern GpItemRec  Gp_ItemTable2[];
extern GpItemRec* Gp_ItemTable1;
/// Overlay string table for item ids `>= 0x500`, indexed by `id - 0x500`
/// (`Gp_GetItemText`).
extern char* Gp_ItemTextHi[];

s32  func_800B7420(s32 arg0);
void Gp_RecalcMaxMp(void);
/// Equips item `arg0` (ids `0x60..0x7F`) as `Wip_SysConfig.field_23`
/// (item id − 0x5F). Marks the new row's `field_1` as −1 and clears the
/// previous selection, then recomputes max HP/MP (same bodies as
/// `Gp_RecalcMaxHp` / `Gp_RecalcMaxMp`), refreshes every inventory row with
/// `Gp_RefreshItemRow`, and sets the collected bit in `Mc_SaveData.field_6D0`.
/// `arg0 == 0` only recomputes HP/MP. Both of those paths copy current
/// HP/MP into `Gp_HpMpWork`; any other id returns without that copy.
void       Gp_EquipMod(s32 arg0);
void       Gp_InitStarterInv(void);
GpItemRec* Gp_GiveItem(GpItemScan* arg0, s32 arg1, s32 arg2);
s32        Gp_RemoveItem(GpItemScan* arg0, GpItemRec* arg1, s32 arg2);
/// Confirmation UI for raising `Mc_SaveData.field_908` of the equipped
/// 0x60–0x7F item (`Wip_SysConfig.field_23`). If the clamped level is
/// already 10, `Gp_NoticePanelTask` is shown with spawnArg1 0x1A. Otherwise
/// consumes `Gp_SelItemRec` and draws "More <item> attachments available."
void Gp_UiBoostAttach(struct _UiObject* arg0, Task* arg1);
void Gp_UiBoostMp(struct _UiObject* arg0, Task* arg1);
/// HP counterpart of `Gp_UiBoostMp`: adds 5 to `Mc_SaveData.field_26`
/// (clamped below 250), recomputes max HP (same body as `Gp_RecalcMaxHp`),
/// heals current HP to that max, then consumes `Gp_SelItemRec` and spawns
/// `Gp_BoostPanelDesc`. `Gp_NoticePanelTask` is called with `spawnArg1` forced to 0x1C.
void Gp_UiBoostHp(struct _UiObject* arg0, Task* arg1);
s32  func_800B9D80(s32 arg0);
/// Unequips `Wip_SysConfig.field_21` (ids 1..32 use the same slot clear as
/// `Gp_ClearEquipSlot`), resets the `Gp_DefaultScan` item table, copies that scan
/// into `Mc_SaveData.field_5BC`, adds one of item 0x6C, heals current HP/MP
/// to max, zeros the 4x3 `Gp_DebugAttachLevels` table, and clears `Gp_StateC08.field_5`
/// / `field_B`.
void Gp_ResetInventory(void);
/// Unequips `Wip_SysConfig.field_21` (same slot clear as `Gp_ResetInventory`),
/// zeros the `Gp_DefaultScan` item table, writes `{0, 0x14, 0}` into
/// `Mc_SaveData.field_5BC`, and if that table has an equipped 0x60–0x7F
/// item (`field_1 == -1`) sets `field_23` and recomputes max HP/MP
/// (`Gp_RecalcMaxHp` / `Gp_RecalcMaxMp`). Heals current HP/MP to max, then
/// clears `Gp_StateC08.field_5` / `field_B`.
void Gp_ClearInventory(void);
void func_800B8014(void);
/// Moves the item at scan slot `arg1` onto slot `arg2`, shifting the
/// occupied rows between them toward the hole left at `arg1`.
void Gp_MoveItemSlot(GpItemScan* arg0, s32 arg1, s32 arg2);
/// Selection-sorts the item table selected by `arg0` using the same
/// sort-key remap as `Gp_ItemSortKey` (`Gp_ItemSortKey0` / `Gp_ItemSortKey60` /
/// `Gp_ItemSortKey80` / `Gp_ItemSortKeyA0`). `arg1` is unused.
void Gp_SortItems(GpItemScan* arg0, s32 arg1);
void Gp_InitModeEquip(void);
void Gp_ApplyBit2Bank(s32 arg0);
void Gp_SetCurBit2Flag(s32 arg0, u8 arg1);
void Gp_ClearScanItems(GpItemScan* arg0);
void Gp_ClearCollectedBits(void);
s32  Gp_CountCollectedBits(void);
s32  Gp_CountScanItems(GpItemScan* arg0);
/// True if `arg2` of item `arg1` can be added to the item table selected
/// by `arg0`. Ids `>= 0x100` always succeed. Ids `0xA0..0xFF` stack onto
/// an existing row when `qty + arg2` fits `Gp_StackLimits[id-0xA0].field_2`;
/// `arg2 < 0` uses that row's `field_0` as the addend. Other ids need a
/// free slot.
s32 Gp_CanAddItemQty(GpItemScan* arg0, s32 arg1, s32 arg2);
/// True if `arg1` can be added to the item table selected by `arg0`.
s32 Gp_CanAddItem(GpItemScan* arg0, s32 arg1);
/// Writes item `arg2` into scan slot `arg1`. Ids `0xA0..0xBF` are added with
/// `Gp_GiveItem` first, then an existing stack is moved onto the slot when
/// it is empty. Other ids overwrite the slot (re-adding the previous item).
GpItemRec* Gp_SetScanItem(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);
/// Adds `arg2` of item `arg1` to the item table selected by `arg0`.
/// Ids `0xA0..0xBF` stack onto an existing row, clamped to
/// `Gp_StackLimits[id-0xA0].field_2`. `arg2 < 0` uses that row's `field_0`
/// as the count, or `field_2` when `arg2 == -2`; out-of-range ids use 1.
/// Other ids take the first free slot with quantity 1. Returns the
/// written row, or NULL if none was free.
GpItemRec* Gp_AddItem(GpItemScan* arg0, s32 arg1, s32 arg2);
/// Returns the `arg1`-th text field of item `arg0` (NUL / `\\n` / `\\N`
/// delimiters). `arg2 == 0` reads `Mc_SaveData.field_6D0` and adds 3 to
/// `arg1` when the bit is clear. Ids `>= 0x500` index `Gp_ItemTextHi`;
/// `0x300..0x4FF` unpack and recurse.
char* Gp_GetItemText(s32 arg0, s32 arg1, s32 arg2);
/// Returns the `arg1`-th matching item id from the table selected by `arg0`.
/// `0x80..0x9F` ids match when `arg2 == 0`, or when `arg2` is a related id
/// in `Gp_RelatedQty0` / `Gp_RelatedQty1` and the row is stocked or selected.
s32         Gp_NthRelatedId(GpItemScan* arg0, s32 arg1, s32 arg2);
void        Gp_RefreshItemRow(GpItemRec* arg0);
GpItemSlot* Gp_GetItemSlot(s32 arg0);
s32         Gp_CountEquippedRelated(GpItemScan* arg0, s32 arg1);
void        Gp_ClearEquipSlot(s32 arg0);
void        Gp_ClearEquipSlotSel(s32 arg0, s32 arg1);
s32         Gp_ScanStackQty(GpItemScan* arg0, s32 arg1);
void        Gp_ConsumeScanQty(GpItemScan* arg0, s32 arg1, s32 arg2);
s32         Gp_FillRelated(s32 arg0, s32 arg1);
s32         Gp_UnequipRelated(s32 arg0, s32 arg1);
s32         Gp_GetCurBit2Flag(s32 arg0);
s32         Gp_HasCollectedBit(s32 arg0);
s32         Gp_AgeFlag119(void);
void        Gp_ClearCollectedBit(s32 arg0);
GpItemRec*  Gp_GetItemTable(GpItemScan* arg0);
s32         Gp_ScanIndexOf(GpItemScan* arg0, GpItemRec* arg1);
/// `arg2` is unused; some callers pass 0 so the `jal` delay slot is `move a2, zero`.
GpItemRec* Gp_GetScanSlot(GpItemScan* arg0, s32 arg1, s32 arg2);
s32        Gp_GetScanItemId(GpItemScan* arg0, s32 arg1);
/// `arg1` is unused; some callers pass 0 so the `jal` delay slot is `move a1, zero`.
s32              Gp_NthCollectedId(s32 arg0, s32 arg1);
s32              Gp_SumScanQty(GpItemScan* arg0, s32 arg1);
void             Gp_SetItemSeenBit(s32 arg0, s32 arg1);
void             Gp_ApplyBit2List(GpBit2List* arg0, u32* arg1);
void             Gp_SetBit2Flag(s32 arg0, u8 arg1, s32 arg2);
s32              Gp_GetBit2Flag(GameSessionFrom4* arg0, s32 arg1);
void             Gp_SavePlayerPos(void);
struct _GpEnemy* Gp_SpawnAtPlace(GpEnemyDesc* arg0, GpEnemyPlace* arg1);
void             Gp_WaitItemFlag2(Task* arg0);
s32              Gp_NextMappedSlot(s32 arg0);
GpItemMap*       Gp_GetItemMap(s32 arg0);
s32              Gp_HasMappedItem(void);
void             Gp_ResetAuxSlots(void);
s32              Gp_SumItemQty(s32 arg0);
void             Gp_SyncHeldRelated(void);
void             Gp_InitItemSeenBits(void);
s32              Gp_HasItemSeenBit(s32 arg0);
void             Gp_RecalcMaxHp(void);
/// Returns `arg0[1]` (e.g. `McItemScan.field_1` capacity).
s32  Gp_GetScanCount(u8* arg0);
s32  Gp_ItemSortKey(s32 arg0);
s32  Gp_GetModLevel(s32 arg0);
void Gp_TickBoostPanel(Task* arg0);
s32  Gp_HasStockedItem(s32 arg0);
s32  Gp_CanMoveItems(void);

#endif // GAMEPLAY_268_H
