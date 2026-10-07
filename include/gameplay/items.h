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

/// Default pack and full-stack quantity requests for `inventoryGiveItem`.
enum {
    INVENTORY_GIVE_ONE_PACK   = -1,
    INVENTORY_GIVE_FULL_STACK = -2
};

/// Adds item units to a range and borrows the affected writable row, or NULL.
///
/// Consumables (0xA0..0xBF) share the first matching stack, capped at its
/// catalogue capacity. Other ids allocate a free row with quantity one.
/// Negative quantities request one pack, except `INVENTORY_GIVE_FULL_STACK`,
/// which requests maximum capacity. `itemId` must be 1..0xFF and the range
/// must fit its writable table. The descriptor is not modified. Sorting,
/// transfers or resetting the backing table can replace the returned item.
InventoryItemRow* inventoryGiveItem(const InventoryItemRange* range, s32 itemId, s32 quantity);

/// Unequips `gPlayerStatus.weapon` (ids 1..32 use the same slot clear as
/// `equipmentClearRemovableLoads`), resets the `Gp_DefaultScan` item table, copies that scan
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

/// Stores one packed two-bit object state in the live save's current stage.
///
/// `objectId` must be 0..63, `state` 0..3, and the saved stage 1..5; none
/// is checked. Higher state bits are not masked and would affect adjacent
/// slots. A packed enemy place key must first be narrowed to its low byte.
/// Night Dryfield shares daytime Dryfield's words. Stage selection follows
/// the live save, independently of the session used by `areaGetCurrentObjectState`.
void areaSetCurrentObjectState(s32 objectId, u8 state);

/// Empties every row in a range by clearing its item id, attachment and quantity.
///
/// The descriptor must be readable and select rows within writable backing
/// storage. Zero rows performs no writes. The descriptor, weapon loads and
/// player equipment selections are left intact. Borrowed row addresses stay
/// valid while their table remains available, but their contents become empty.
void inventoryClearItems(const InventoryItemRange* range);

s32 Gp_CountScanItems(InventoryItemRange* arg0);

/// Borrows the live save's writable load record for a weapon item id.
///
/// `weaponItemId` must be 0x80..0x9F; there is no bounds check. The record is
/// indexed by item id, independently of inventory position or equipment.
/// Loading or resetting the live save can replace its contents.
EquipmentWeaponLoad* equipmentGetWeaponLoad(s32 weaponItemId);

s32 Gp_ScanStackQty(InventoryItemRange* arg0, s32 arg1);

/// Returns one packed two-bit object state from the session's current stage.
///
/// `objectId` must be 0..63 and the current stage must be 1..5. Neither is
/// checked. A packed enemy place key must first be narrowed to its low byte.
/// The result is 0..3; ordinary pickup completion stores 2 unless it is 3.
/// Night Dryfield shares daytime Dryfield's words. Stage selection follows
/// the current session, whereas `areaSetCurrentObjectState` follows the live save.
s32 areaGetCurrentObjectState(s32 objectId);

/// Returns 1 if a live-save collection bit is set, otherwise 0.
///
/// Only the low seven bits of `collectionId` select the bit. Key-item ids
/// 0x100..0x17F and their zero-based bit indices therefore address the same
/// 128-bit set, which also records item-related events.
s32 inventoryHasCollectedBit(s32 collectionId);

/// Catalogue ids used as live-save collection bits for items and rescue bonuses.
///
/// The collection APIs keep each id's low seven bits.
enum {
    INVENTORY_COLLECTION_ID_PARTHENON_KEY        = 0x101,
    INVENTORY_COLLECTION_ID_RED_KEY              = 0x103,
    INVENTORY_COLLECTION_ID_BLUE_KEY             = 0x104,
    INVENTORY_COLLECTION_ID_ARMORY_CARDKEY       = 0x105,
    INVENTORY_COLLECTION_ID_MIST_BADGE           = 0x106,
    INVENTORY_COLLECTION_ID_MENDEL_JOURNAL       = 0x107,
    INVENTORY_COLLECTION_ID_MIST_SEARCH_WARRANT  = 0x109,
    INVENTORY_COLLECTION_ID_NMC_PHOTO            = 0x10A,
    INVENTORY_COLLECTION_ID_MANUAL               = 0x10B,
    INVENTORY_COLLECTION_ID_DRYFIELD_MAP         = 0x10C,
    INVENTORY_COLLECTION_ID_MOTEL_ROOM_6_KEY     = 0x10F,
    INVENTORY_COLLECTION_ID_MONKEY_WRENCH        = 0x111,
    INVENTORY_COLLECTION_ID_LOBBY_KEY            = 0x112,
    INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY     = 0x113,
    INVENTORY_COLLECTION_ID_WIRE_ROPE            = 0x114,
    INVENTORY_COLLECTION_ID_FACTORY_KEY          = 0x115,
    INVENTORY_COLLECTION_ID_TRUCK_KEY            = 0x116,
    INVENTORY_COLLECTION_ID_JERRY_CAN            = 0x117,
    INVENTORY_COLLECTION_ID_GASOLINE             = 0x118,
    INVENTORY_COLLECTION_ID_ICE_BAG              = 0x119,
    INVENTORY_COLLECTION_ID_BAG_OF_WATER         = 0x11A,
    INVENTORY_COLLECTION_ID_BOTTLECAP_MAGNET     = 0x11B,
    INVENTORY_COLLECTION_ID_SUV_KEY              = 0x11E,
    INVENTORY_COLLECTION_ID_OAK_BOARD            = 0x11F,
    INVENTORY_COLLECTION_ID_BOWMANS_CARD         = 0x121,
    INVENTORY_COLLECTION_ID_PIERCE_RESCUE_BONUS  = 0x12F,
    INVENTORY_COLLECTION_ID_SOLDIER_RESCUE_BONUS = 0x130
};

/// Clears the live-save collection bit selected by an id's low seven bits.
///
/// Accepts the same key-item ids or bit indices as `inventoryHasCollectedBit`.
/// The play-time marker is left intact.
void inventoryClearCollectedBit(s32 collectionId);

/// Borrows the writable table selected by a range, before its first-row offset.
///
/// Table id 1 selects the current indirect table, 2 the gameplay area-grant
/// rows, and every other value the live save's rows. The descriptor must be
/// readable and the selected table must remain available while used. Saved
/// storage lasts with the resident save, area-grant storage with gameplay;
/// the indirect table's owner determines its lifetime. No rows are inspected.
InventoryItemRow* inventoryGetRangeTable(const InventoryItemRange* range);

/// Sets the live-save collection bit selected by an id's low seven bits.
///
/// Accepts the same key-item ids or bit indices as `inventoryHasCollectedBit`.
/// Setting the Ice Bag bit (0x19, item id 0x119) also refreshes its play-time
/// marker from the live save, even if the bit was already set.
void inventorySetCollectedBit(s32 collectionId);

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

/// Returns 1 if an item has a free row or its first consumable stack has room.
///
/// Existing consumables need room for at least one unit, independently of pack
/// size or a pickup's quantity. A full first stack returns 0 even with free
/// rows. A missing stack or other row item needs one free row. Ids >= 0x100
/// return 1 after scanning the rows, without testing collection-bit possession.
/// Row-item ids must be 1..0xBF; reserved ids 0xC0..0xFF are not rejected and
/// would index beyond the 32-row consumable catalogue if a matching row exists.
/// The range must fit readable backing storage. Neither input is changed or
/// retained. This is a capacity query; it does not reserve space or add items.
s32 inventoryCanAddItem(const InventoryItemRange* range, s32 itemId);

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
