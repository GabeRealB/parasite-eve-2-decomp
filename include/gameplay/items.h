#ifndef GAMEPLAY_ITEMS_H
#define GAMEPLAY_ITEMS_H

#include "types.h"

#include "gameplay/inventory.h"

#include "main/mc_types.h"
#include "main/task_types.h"

// Inventory contents, collection flags, quantities, sorting and equipment.

extern PlayerModeBaseStats Gp_StatRows[];

void Gp_ApplyItemMap(void);

s32 Gp_ConsumeSlotQty(s32 arg0, s32 arg1);

/// Equips related item `arg2` (ids `0xA0..0xBF`) onto save-slot `arg1`
/// (ids `0x80..0x9F`) in the table selected by `arg0`. Tries `Gp_QtyById0`
/// then `Gp_QtyById1` for a matching related id. `arg3 < 0` uses that
/// row's max qty. Returns the stored count, 0 if `arg3 == 0`, or -1.
s32 Gp_EquipRelatedItem(InventoryItemRange* arg0, s32 arg1, s32 arg2, s32 arg3);

s32 func_800B7420(s32 arg0);

void Gp_RecalcMaxMp(void);

/// Equips item `arg0` (ids `0x60..0x7F`) as `gPlayerStatus.armor`
/// (item id − 0x5F). Marks the new row's `field_1` as −1 and clears the
/// previous selection, then recomputes max HP/MP (same bodies as
/// `Gp_RecalcMaxHp` / `Gp_RecalcMaxMp`), refreshes every inventory row with
/// `inventoryDetachItem`, and sets the collected bit in `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemSeenBits`.
/// `arg0 == 0` only recomputes HP/MP. Both of those paths copy current
/// HP/MP into `Gp_HpMpWork`; any other id returns without that copy.
void Gp_EquipMod(s32 arg0);

InventoryItemRow* Gp_GiveItem(InventoryItemRange* arg0, s32 arg1, s32 arg2);

/// Unequips `gPlayerStatus.weapon` (ids 1..32 use the same slot clear as
/// `Gp_ClearEquipSlot`), resets the `Gp_DefaultScan` item table, copies that scan
/// into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems`, adds one of item 0x6C, heals current HP/MP
/// to max, zeros the 4x3 `Gp_DebugAttachLevels` table, and clears `Gp_StateC08.activeIndex`
/// / `wheelIndex`.
void Gp_ResetInventory(void);

/// Unequips `gPlayerStatus.weapon` (same slot clear as `Gp_ResetInventory`),
/// zeros the `Gp_DefaultScan` item table, writes `{0, 0x14, 0}` into
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems`, and if that table has an equipped 0x60–0x7F
/// item (`field_1 == -1`) sets `field_23` and recomputes max HP/MP
/// (`Gp_RecalcMaxHp` / `Gp_RecalcMaxMp`). Heals current HP/MP to max, then
/// clears `Gp_StateC08.activeIndex` / `wheelIndex`.
void Gp_ClearInventory(void);

void Gp_SetCurBit2Flag(s32 arg0, u8 arg1);

void Gp_ClearScanItems(InventoryItemRange* arg0);

s32 Gp_CountScanItems(InventoryItemRange* arg0);

EquipmentWeaponLoad* Gp_GetItemSlot(s32 arg0);

s32 Gp_ScanStackQty(InventoryItemRange* arg0, s32 arg1);

s32 Gp_GetCurBit2Flag(s32 arg0);

s32 Gp_HasCollectedBit(s32 arg0);

void Gp_ClearCollectedBit(s32 arg0);

InventoryItemRow* Gp_GetItemTable(InventoryItemRange* arg0);

void Gp_SetCollectedBit(s32 arg0);

s32 Gp_AgeFlag119(void);

s32 Gp_SumScanQty(InventoryItemRange* arg0, s32 arg1);

void Gp_SetItemSeenBit(s32 arg0, s32 arg1);

void Gp_SetBit2Flag(s32 arg0, u8 arg1, s32 arg2);

s32 Gp_NextMappedSlot(s32 arg0);

EquipmentWeaponSupply* Gp_GetItemMap(s32 arg0);

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

extern ItemDesc Gp_ItemDescs[];

/// Item descriptors for ids from 0x100 up, indexed by `id - 0x100`.
/// Key-item rows, indexed by item id minus 0x100.
extern ItemDesc Gp_KeyItemDescs[];

/// Weapon quantity/related-id rows, indexed by item id minus 0x80.
extern EquipmentWeaponLoadOptionsTable Gp_RelatedQty0;

extern InventoryConsumableStack Gp_StackLimits[];

/// Pack size and stack capacity of consumable `itemId` (0xA0..0xBF).
static inline InventoryConsumableStack* gpItemStock(s32 itemId)
{
    return &Gp_StackLimits[itemId - 0xA0];
}

/// True if `arg1` can be added to the item table selected by `arg0`.
s32 Gp_CanAddItem(InventoryItemRange* arg0, s32 arg1);

/// Catalogue field indices and raw id dispatch boundaries for `itemGetText`.
enum {
    ITEM_TEXT_NAME               = 0,
    ITEM_TEXT_DESCRIPTION_FIRST  = 1,
    ITEM_TEXT_DESCRIPTION_SECOND = 2,
    ITEM_TEXT_FIELDS_PER_FORM    = 3,
    ITEM_TEXT_KEY_ID_FIRST       = 0x100,
    ITEM_TEXT_PACKED_ID_FIRST    = 0x300,
    ITEM_TEXT_ENEMY_ID_FIRST     = 0x500
};

/// Borrows the encoded name or description suffix for an item or packed PE id.
///
/// Nonnegative `fieldIndex` selects name (0) or description line (1/2); values >= 3 select
/// the name. Zero `forceIdentified` consults the live save's identification bit;
/// nonzero always selects identified text. Fields are separated by NUL, newline
/// or a backslash followed by n/N. The suffix is not copied or terminated anew.
///
/// Declared catalogue ids are 0..0xBF and 0x100..0x17F; raw id dispatch performs
/// no bounds check. Packed ids 0x300..0x4FF use element/energy/level bits to select
/// an identified ordinary entry. Enemy ids 0x500..0x53E use `Gp_ItemTextHi` and
/// ignore the field/form arguments; the enemy-name overlay must be loaded.
/// Results remain readable while their defining image stays loaded. Skipping
/// fields requires all delimiters to be readable; a first byte n/N also causes
/// the retained parser to read the preceding byte.
const u8* itemGetText(s32 itemId, s32 fieldIndex, s32 forceIdentified);

#endif // GAMEPLAY_ITEMS_H
