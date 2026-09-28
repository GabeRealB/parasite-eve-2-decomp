#ifndef GAMEPLAY_ITEMS_H
#define GAMEPLAY_ITEMS_H

#include "types.h"

#include "gameplay/inventory.h"

#include "main/mc_types.h"
#include "main/task_types.h"

// Inventory contents, collection flags, quantities, sorting and equipment.

extern GpStatRow Gp_StatRows[];

void Gp_ApplyItemMap(void);

s32 Gp_ConsumeSlotQty(s32 arg0, s32 arg1);

/// Equips related item `arg2` (ids `0xA0..0xBF`) onto save-slot `arg1`
/// (ids `0x80..0x9F`) in the table selected by `arg0`. Tries `Gp_QtyById0`
/// then `Gp_QtyById1` for a matching related id. `arg3 < 0` uses that
/// row's max qty. Returns the stored count, 0 if `arg3 == 0`, or -1.
s32 Gp_EquipRelatedItem(McItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);

s32 func_800B7420(s32 arg0);

void Gp_RecalcMaxMp(void);

/// Equips item `arg0` (ids `0x60..0x7F`) as `Player_Status.armor`
/// (item id − 0x5F). Marks the new row's `field_1` as −1 and clears the
/// previous selection, then recomputes max HP/MP (same bodies as
/// `Gp_RecalcMaxHp` / `Gp_RecalcMaxMp`), refreshes every inventory row with
/// `Gp_RefreshItemRow`, and sets the collected bit in `Mc_SaveData[0].state.itemSeenBits`.
/// `arg0 == 0` only recomputes HP/MP. Both of those paths copy current
/// HP/MP into `Gp_HpMpWork`; any other id returns without that copy.
void Gp_EquipMod(s32 arg0);

McItemRec* Gp_GiveItem(McItemScan* arg0, s32 arg1, s32 arg2);

/// Unequips `Player_Status.weapon` (ids 1..32 use the same slot clear as
/// `Gp_ClearEquipSlot`), resets the `Gp_DefaultScan` item table, copies that scan
/// into `Mc_SaveData[0].state.carriedItems`, adds one of item 0x6C, heals current HP/MP
/// to max, zeros the 4x3 `Gp_DebugAttachLevels` table, and clears `Gp_StateC08.field_5`
/// / `field_B`.
void Gp_ResetInventory(void);

/// Unequips `Player_Status.weapon` (same slot clear as `Gp_ResetInventory`),
/// zeros the `Gp_DefaultScan` item table, writes `{0, 0x14, 0}` into
/// `Mc_SaveData[0].state.carriedItems`, and if that table has an equipped 0x60–0x7F
/// item (`field_1 == -1`) sets `field_23` and recomputes max HP/MP
/// (`Gp_RecalcMaxHp` / `Gp_RecalcMaxMp`). Heals current HP/MP to max, then
/// clears `Gp_StateC08.field_5` / `field_B`.
void Gp_ClearInventory(void);

void Gp_SetCurBit2Flag(s32 arg0, u8 arg1);

void Gp_ClearScanItems(McItemScan* arg0);

s32 Gp_CountScanItems(McItemScan* arg0);

McItemSlot* Gp_GetItemSlot(s32 arg0);

s32 Gp_ScanStackQty(McItemScan* arg0, s32 arg1);

s32 Gp_GetCurBit2Flag(s32 arg0);

s32 Gp_HasCollectedBit(s32 arg0);

void Gp_ClearCollectedBit(s32 arg0);

McItemRec* Gp_GetItemTable(McItemScan* arg0);

void Gp_SetCollectedBit(s32 arg0);

s32 Gp_AgeFlag119(void);

s32 Gp_SumScanQty(McItemScan* arg0, s32 arg1);

void Gp_SetItemSeenBit(s32 arg0, s32 arg1);

void Gp_SetBit2Flag(s32 arg0, u8 arg1, s32 arg2);

s32 Gp_NextMappedSlot(s32 arg0);

GpItemMap* Gp_GetItemMap(s32 arg0);

s32 Gp_HasMappedItem(void);

void Gp_RecalcMaxHp(void);

s32 Gp_ItemSortKey(s32 arg0);

s32 Gp_HasStockedItem(s32 arg0);

void Gp_MarkPlayTime(void);

s32 Gp_GetRelatedQty(s32 arg0, s32 arg1);

void Gp_FillHpMp(void);

void func_800BC4BC(void);

void func_800BC4E4(void);

void Gp_ResetScanDefault(void);

/// Named as a task entry by the enemy descriptor tables in the map UI overlays.
void Gp_WaitItemFlag2(Task* arg0);

extern GpItemDesc Gp_ItemDescs[];

/// Item descriptors for ids from 0x100 up, indexed by `id - 0x100`.
/// Key-item rows, indexed by item id minus 0x100.
extern GpItemDesc Gp_KeyItemDescs[];

/// Weapon quantity/related-id rows, indexed by item id minus 0x80.
extern GpRelatedItemTable Gp_RelatedQty0;

extern GpItemA0 Gp_StackLimits[];

/// Quantity limits for an ammunition item (ids 0xA0–0xBF).
static inline GpItemA0* gpItemStock(s32 itemId)
{
    return &Gp_StackLimits[itemId - 0xA0];
}

/// True if `arg1` can be added to the item table selected by `arg0`.
s32 Gp_CanAddItem(McItemScan* arg0, s32 arg1);

/// Returns the `arg1`-th text field of item `arg0` (NUL / `\\n` / `\\N`
/// delimiters). `arg2 == 0` reads `Mc_SaveData[0].state.itemSeenBits` and adds 3 to
/// `arg1` when the bit is clear. Ids `>= 0x500` index `Gp_ItemTextHi`;
/// `0x300..0x4FF` unpack and recurse.
char* Gp_GetItemText(s32 arg0, s32 arg1, s32 arg2);

#endif // GAMEPLAY_ITEMS_H
