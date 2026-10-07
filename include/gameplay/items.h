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
/// `equipmentRecalculateMaxHp` / `Gp_RecalcMaxMp`), refreshes every inventory row with
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
/// (`equipmentRecalculateMaxHp` / `Gp_RecalcMaxMp`). Heals current HP/MP to max, then
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

/// Counts occupied row slots in an inventory range, independently of quantity.
///
/// Only a nonzero item id marks an occupied row, including zero-quantity rows.
/// The descriptor must select rows within its readable backing table; zero
/// rows returns zero. The result is 0..rowCount. Inputs are borrowed unchanged.
s32 inventoryCountOccupiedRows(const InventoryItemRange* range);

/// Borrows the live save's writable load record for a weapon item id.
///
/// `weaponItemId` must be 0x80..0x9F; there is no bounds check. The record is
/// indexed by item id, independently of inventory position or equipment.
/// Loading or resetting the live save can replace its contents.
EquipmentWeaponLoad* equipmentGetWeaponLoad(s32 weaponItemId);

/// Returns the first matching consumable stack's quantity as a signed count.
///
/// Consumable ids 0xA0..0xBF search the range; other ids or no match return zero.
/// The stored u16 narrows to s16 before promotion to s32. Duplicate stacks are
/// not summed, unlike `inventoryGetItemQuantity`. Loaded ammunition remains in
/// the row's total; no weapon loads are subtracted. The readable descriptor
/// must select rows within its backing table. Inputs are borrowed unchanged.
s32 inventoryGetConsumableStackQuantity(const InventoryItemRange* range, s32 consumableItemId);

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

/// Converts a held Ice Bag to a Bag of Water when its two-minute timer expires.
///
/// Returns 1 only when the collection bits change, otherwise 0. Elapsed saved
/// minutes narrow to signed 16 bits before comparison; the marker is retained.
/// Collecting an Ice Bag or `inventoryResetIceBagTimer` restarts the timer.
s32 inventoryMeltIceBagIfExpired(void);

/// Sums matching row quantities, or returns a key item's collection bit.
///
/// Row ids below 0x100 use the descriptor's selected table and include loaded
/// ammunition in the total. The range must fit readable backing storage; zero
/// rows returns zero. Ids >= 0x100 instead query the collection flag
/// selected by their low seven bits, without reading the range. Inputs
/// are borrowed and left unchanged.
s32 inventoryGetItemQuantity(const InventoryItemRange* range, s32 itemId);

/// Sets or clears an item's persistent catalogue identification flag.
///
/// `itemId` must be nonnegative; ids >= 0x180 are ignored. Zero `identified`
/// clears the flag and any nonzero value sets it. Identification controls the
/// name, description and icon independently of possession or collection.
void itemSetIdentified(s32 itemId, s32 identified);

void Gp_SetBit2Flag(s32 arg0, u8 arg1, s32 arg2);

/// No carried weapon with a built-in supply was found.
enum { EQUIPMENT_WEAPON_SUPPLY_NOT_FOUND = -1 };

/// Finds the next built-in supply whose weapon is in the live carried range.
///
/// Starts at `firstSupplyIndex` inclusively. Returns a catalogue index 0..7,
/// or `EQUIPMENT_WEAPON_SUPPLY_NOT_FOUND` for an invalid start or no match.
/// The carried range must fit its readable table; neither it nor loads change.
s32 equipmentFindNextCarriedWeaponSupply(s32 firstSupplyIndex);

/// Borrows one read-only row of the built-in weapon-supply catalogue.
///
/// `supplyIndex` must be 0..7; there is no bounds check. The row remains valid
/// while gameplay is loaded and names the weapon, supply item and load to refill.
const EquipmentWeaponSupply* equipmentGetWeaponSupply(s32 supplyIndex);

s32 Gp_HasMappedItem(void);

/// Recomputes maximum HP from the game mode, permanent bonus and equipped armor.
///
/// The live save's mode must be 0..3 and equipped armor 0..32. Each addition
/// narrows through an unsigned 16-bit accumulator before the signed maximum
/// is capped at 250. Current HP is reduced only when it exceeds the new maximum.
void equipmentRecalculateMaxHp(void);

/// Returns a catalogue order key, an id-based fallback or the empty-row key.
///
/// Remapped row ids use their category table; a zero remap or other id falls
/// back to `itemId + 0x100`. Id zero returns 0x1000. Remap inputs must be
/// 1..0x47, 0x60..0x6E or 0x80..0xBF; unused ordinary/armor gaps outside those
/// ranges exceed the declared tables despite passing the raw category tests.
/// The shop's 0xFFFE recharge-service id uses the numerical fallback.
s32 inventoryGetItemSortKey(s32 itemId);

s32 Gp_HasStockedItem(s32 arg0);

/// Restarts the Ice Bag's melting timer at the live save's whole play minutes.
///
/// Room events use this to hold or restart melting. Collection bits are left
/// intact; collecting an Ice Bag also refreshes this marker.
void inventoryResetIceBagTimer(void);

/// Returns the maximum rounds or supply units in one weapon load.
///
/// Weapon item ids 0x80..0x9F index the catalogue; other ids return zero.
/// Zero `loadSelection` selects primary and every nonzero value secondary.
/// The result is a capacity, independently of possession or remaining charge.
s32 equipmentGetWeaponLoadCapacity(s32 weaponItemId, s32 loadSelection);

/// Restores current HP and MP to their already computed maxima.
void equipmentRestoreHpMp(void);

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
