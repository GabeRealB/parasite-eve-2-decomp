#ifndef GAMEPLAY_PRIVATE_ITEMS_H
#define GAMEPLAY_PRIVATE_ITEMS_H

#include "types.h"

#include "gameplay/inventory.h"
#include "inventory.h"

#include "main/mc_types.h"
#include "main/task_types.h"
#include "main/ui_types.h"

// Inventory contents, collection flags, quantities, sorting and equipment.

/// Qty table indexed by raw item id. `Gp_RelatedQty1` is the 0x80–0x9F slice
/// at +0x200 (`equipmentLoadCarriedWeaponConsumable`).
extern EquipmentWeaponLoadOptionsTable Gp_RelatedQty1;

extern EquipmentWeaponSupply Gp_ItemMaps[];

extern InventoryItemRow* Gp_ItemTable1;

extern const char Gp_StrNotice2[8];

/// Loads or checks a consumable in a selected load of a carried weapon.
///
/// Zero selection means primary; every nonzero selection means secondary.
/// Weapon ids outside 0x80..0x9F, absent weapons or items outside that load's
/// catalogue choices return -1. The carried range must fit its readable table.
/// Stock is the first stack's signed quantity minus carried weapon loads, plus
/// this selected load's quantity when it already uses the consumable.
/// Negative requests use capacity; positive requests clamp to capacity and stock.
/// Zero checks positive stock without writing, even for an unavailable secondary
/// load. A positive write to an unavailable secondary returns -1; other successes
/// return the clamped quantity. Inventory totals and identification stay intact.
s32 equipmentLoadCarriedWeaponConsumable(s32 loadSelection, s32 weaponItemId, s32 consumableItemId, s32 requestedQuantity);

/// Quantity request that consumes the whole first matching stack.
enum { INVENTORY_REMOVE_WHOLE_STACK = -1 };

/// Removes a separate item row or consumes its first matching quantity stack.
///
/// Ids below 0xA0 clear `row`'s id, quantity and attachment, ignoring quantity
/// and permitting NULL `range`. Other ids search the range's first matching
/// row, which can differ from `row`: negative quantities consume it all,
/// excess consumption clamps to zero, and zero remainder clears its fields.
/// A missing match changes nothing. The row must be writable and any used
/// range must fit its writable table. Weapon loads are left intact. Returns 0.
s32 inventoryRemoveItemRow(InventoryItemRange* range, InventoryItemRow* row, s32 quantity);

/// Applies a Belt Pouch to the equipped armor and shows the attachment-slot notice.
///
/// The equipped armor selector must be 1..32 and remain unchanged while open;
/// object and its owner task must remain live. State 0 adds one permanent bonus
/// slot below the ten-slot cap and consumes the selected writable pouch row in
/// `Gp_SelItemRec`. At the cap it shows "No further modifications" without consuming.
/// Success counts down 188 active updates; timeout or Confirm/Cancel yields
/// DISMISS, and Menu yields CANCEL. The capped path uses `itemMenuNoticeTask`'s
/// countdown contract. The task's item-id spawn argument is preserved.
void itemMenuApplyPouchPanel(UiObject* object, Task* task);

/// Applies the selected permanent MP boost and displays its timed notice.
///
/// On state zero, adds one to the saved MP bonus if below 250, recalculates
/// maximum MP, restores current MP and consumes the selected writable item row.
/// Captures prior HP/MP for the child statistics panel. The selected item must
/// be a non-consumable row id below 0xA0; no inventory range is supplied.
/// Mode must be 0..3 and the armor selector 0..32 for maximum-MP recalculation.
/// Object and its owner task must remain live under `itemMenuNoticeTask`'s
/// contract. Every call temporarily selects the MP-increased notice, yielding
/// DISMISS or CANCEL, and restores the task's whole item-id spawn argument.
void itemMenuApplyMpBoostPanel(UiObject* object, Task* task);

/// Applies the selected permanent HP boost and displays its timed notice.
///
/// On state zero, adds five to the saved byte-sized HP bonus if below 250;
/// the addend is not clamped to 250. Recalculates maximum HP with unsigned
/// 16-bit intermediate sums and a signed 250-point cap, then restores current
/// HP and consumes the selected writable item row. Mode must be 0..3 and armor
/// selector 0..32. Captures prior HP/MP for the child statistics panel.
/// The selected item must have a row id below 0xA0; no range is supplied.
/// Object and its owner task must remain live under `itemMenuNoticeTask`'s
/// contract. Every call selects the HP-increased notice, yielding DISMISS or
/// CANCEL, and restores the task's whole item-id spawn argument.
void itemMenuApplyHpBoostPanel(UiObject* object, Task* task);

/// Exact selectors for effects supplied by armour, attached items or active wards.
///
/// These are individual query ids, not combinable flags or PlayerStatus masks.
/// The timed status at player-status bit 0x20 has an unproven gameplay role;
/// its resistance selector always returns zero.
enum {
    EQUIPMENT_EFFECT_RESIST_DARKNESS        = 0x101,
    EQUIPMENT_EFFECT_RESIST_PARALYSIS       = 0x102,
    EQUIPMENT_EFFECT_RESIST_POISON          = 0x104,
    EQUIPMENT_EFFECT_RESIST_SILENCE         = 0x108,
    EQUIPMENT_EFFECT_RESIST_TIMED_STATUS_20 = 0x110,
    EQUIPMENT_EFFECT_RESIST_CONFUSION       = 0x120,
    EQUIPMENT_EFFECT_RESIST_BERSERKER       = 0x140,
    EQUIPMENT_EFFECT_RESIST_IMPACT          = 0x200,
    EQUIPMENT_EFFECT_ARMOR_MOTION_DETECTOR  = 0x400,
    EQUIPMENT_EFFECT_MP_GENERATION          = 0x800,
    EQUIPMENT_EFFECT_HP_RECOVERY            = 0x1000,
    EQUIPMENT_EFFECT_QUICK_FIRE             = 0x2000,
    EQUIPMENT_EFFECT_MEDICAL_INSPECTION     = 0x4000,
    EQUIPMENT_EFFECT_MP_RECOVERY            = 0x8000,
    EQUIPMENT_EFFECT_SKULL_CRYSTAL          = 0x10000,
    EQUIPMENT_EFFECT_OFUDA                  = 0x20000,
    EQUIPMENT_EFFECT_HOLY_WATER             = 0x40000,
    EQUIPMENT_EFFECT_MEDICINE_WHEEL         = 0x80000,
    EQUIPMENT_EFFECT_MOTION_DETECTOR        = 0x100000
};

/// Returns whether one equipment effect is available to the player.
///
/// Reads equipped armour features, positive attachment slots in the carried
/// range, and active Metabolism/body/mind wards. Item quantity is not tested.
/// `effectSelector` is one exact `EQUIPMENT_EFFECT_*` id; unknown ids return 0.
/// The armour-only motion-detector query controls radar zoom, while the general
/// query also accepts attached GPS. Returns 0 or 1 without modifying state.
s32 equipmentHasEffect(s32 effectSelector);

/// Seeds a stage's saved object states from its room placement lists.
///
/// Stage ids 0..5 select the stage table without a bounds check; stage 0
/// has no lists and changes nothing. Night Dryfield is skipped because it
/// shares daytime Dryfield's state words. Listed slots receive their initial
/// two-bit states; every unlisted slot keeps its existing value. The stage's
/// map tables must be loaded, terminated and fit the saved 64-slot bank.
void areaSeedStageObjectStates(s32 stageId);

/// Counts all set bits in the live save's 128-bit collection set (0..128).
///
/// Counts item-related event bits as well as key-item possession bits.
s32 inventoryCountCollectedBits(void);

/// Sums a consumable's primary and secondary loads in the range's weapon rows.
///
/// Consumable ids 0xA0..0xBF count rounds or supply units; other ids return zero.
/// Every weapon row counts, independently of attachment or equipped selection;
/// duplicate weapon ids count the same saved load again. Loads come from the
/// live save even for indirect or area-grant ranges. The readable descriptor
/// must select rows within its backing table. No input is changed or retained.
s32 equipmentGetLoadedConsumableQuantity(const InventoryItemRange* range, s32 consumableItemId);

/// Clears removable weapon loads while preserving any built-in Battery or Fuel.
///
/// Weapon ids outside 0x80..0x9F are ignored. Cleared loads lose their item id
/// and quantity; an unavailable secondary slot keeps its marker. Built-in
/// supply selection and charge, inventory quantities and the saved record's
/// unproven word are left intact. Records remain owned by the live save.
void equipmentClearRemovableLoads(s32 weaponItemId);

/// Selects a carried weapon and detaches its inventory row from any armor slot.
///
/// `weaponItemId` must be 0x80..0x9F with a matching carried row. The previous
/// equipped weapon, if any, must also have a carried row. Selecting the same
/// weapon does nothing. Otherwise the previous row inherits a positive armor
/// slot from the new row; without one, its removable loads are cleared.
/// Updates the player's one-based weapon selector and marks the new item identified.
void equipmentEquipCarriedWeapon(s32 weaponItemId);

/// Which weapon loads `equipmentClearSelectedRemovableLoads` clears.
enum {
    EQUIPMENT_CLEAR_LOAD_BOTH      = 0,
    EQUIPMENT_CLEAR_LOAD_PRIMARY   = 1,
    EQUIPMENT_CLEAR_LOAD_SECONDARY = 2
};

/// Clears selected removable weapon loads while preserving built-in supplies.
///
/// Selection 1 clears primary, 2 secondary, and every other value clears both.
/// Weapon ids outside 0x80..0x9F are ignored. Cleared loads lose their item id
/// and quantity, except that an unavailable secondary slot keeps its marker.
/// The saved record remains owned by the live save; inventory totals are intact.
void equipmentClearSelectedRemovableLoads(s32 weaponItemId, s32 loadSelection);

/// Consumes units from the first matching item row in a writable range.
///
/// A negative quantity consumes the whole first row; excess consumption clamps
/// to zero. Zero remainder clears its id, quantity and attachment. A missing
/// item changes nothing. The range must fit its table. Quantity counts item
/// units, including loaded ammunition; weapon-load records are left intact.
/// The range descriptor is not modified and no pointer is retained.
void inventoryConsumeFirstStack(InventoryItemRange* range, s32 itemId, s32 quantity);

/// Reloads the consumable currently selected in one of a carried weapon's loads.
///
/// Weapon item id must be 0x80..0x9F before the unchecked saved-record lookup.
/// Zero selection reads primaryItemId; every nonzero selection reads secondary.
/// Uses the live carried range and `equipmentLoadWeaponConsumable`'s capacity,
/// stock and primary-preference rules. Returns loaded units, or -1 on failure;
/// empty or unavailable selections fail. Inventory quantities stay intact.
s32 equipmentReloadSelectedWeaponConsumable(s32 weaponItemId, s32 loadSelection);

/// Returns 1 if a selected weapon consumable has compatible positive stock, else 0.
///
/// Weapon item id must be 0x80..0x9F before the unchecked saved-record lookup.
/// Zero selection reads primaryItemId; every nonzero selection reads secondary.
/// Checks the live carried range through `equipmentLoadWeaponConsumable`,
/// including the selected weapon's existing loads in available stock. A full
/// load can qualify. Empty/unavailable selections fail. Changes no load, row
/// or identification flag and retains no pointer.
s32 equipmentCanReloadSelectedWeaponConsumable(s32 weaponItemId, s32 loadSelection);

/// Result of a row-address lookup that finds no row within its range.
enum { INVENTORY_ROW_NOT_FOUND = -1 };

/// Returns a row's zero-based position in a range, or INVENTORY_ROW_NOT_FOUND.
///
/// Compares addresses, including free rows, without inspecting their contents.
/// The range must fit its backing table. NULL or a row outside the range is
/// absent; a row with the same item elsewhere is not a match. Neither input
/// is modified or retained.
s32 inventoryIndexOfRow(const InventoryItemRange* range, const InventoryItemRow* row);

/// Borrows the writable row at a zero-based position within a range.
///
/// The position must be nonnegative and less than rowCount, and the range must
/// fit its backing table; no bounds check is performed. The descriptor is
/// unchanged. The row lasts with its table, but sorting or transfers may replace
/// its item. `unused` is ignored and is retained for the caller's argument setup.
InventoryItemRow* inventoryGetRow(const InventoryItemRange* range, s32 rowIndex, s32 unused);

/// Equips a carried M93R when no weapon is selected and refills its 9mm P.B. load.
///
/// Existing weapon selections stay intact. An equipped M93R is reloaded only
/// when its primary selection is empty or already 9mm P.B.; other ammunition
/// stays selected. The live carried range must fit its readable table and
/// equipment selection must be 0..32. Equipping borrows writable carried rows;
/// reload failure is ignored and inventory stock is not consumed.
void equipmentEnsureM93rEquipped(void);

/// Clears all 128 collection bits in the live save.
///
/// Key-item possession and item-related event bits are reset together. The
/// separate Ice Bag play-time marker and inventory rows are left intact.
void inventoryClearCollectedBits(void);

/// Returns the item id at a zero-based position among held listed key items.
///
/// Only set collection bits in the ordered, terminated key-item list count.
/// `collectedIndex` must be nonnegative; an exhausted list returns
/// `INVENTORY_ITEM_NONE`. `unused` is ignored and preserves the call interface.
s32 inventoryGetCollectedItemId(s32 collectedIndex, s32 unused);

/// Angle units used when capturing the saved player facing (4096 per turn).
enum {
    PLAYER_YAW_HALF_TURN = 0x800,
    PLAYER_YAW_FULL_TURN = 0x1000
};

/// Captures the live player pose and progression before presenting a save prompt.
///
/// Requires the player task's model root. The resident pose receives XYZ in
/// signed 16-bit game coordinates and yaw in 4096 units per turn, within
/// -2048..2048 inclusive. Experience and BP are copied to the live save.
void playerCaptureSaveState(void);

/// Synchronizes the equipped weapon's primary attack selector and collision key.
///
/// Requires a live player actor, weapon selection 0..32 and its saved load.
/// An absent weapon or empty primary selects zero; otherwise the primary item
/// id minus 0x9F is stored modulo 256. Quantity and secondary load are ignored.
/// Inventory rows and saved loads remain intact.
void equipmentSyncPrimaryAttackSelector(void);

/// Returns whether an item uses its identified catalogue text and icon.
///
/// `itemId` must be nonnegative. Ids below 0x180 consult the live save's flag;
/// larger ids return true. Items with no unidentified text start identified,
/// and examination, use or story events can identify others.
bool itemIsIdentified(s32 itemId);

/// Returns the range's number of row slots, including free rows.
///
/// The descriptor must be readable; the result is 0..255. Its backing table
/// is not inspected and the descriptor is left unchanged.
s32 inventoryGetRangeCapacity(const InventoryItemRange* range);

/// Returns an armor item's base plus saved bonus attachment slots, capped at ten.
///
/// Armor item ids 0x60..0x7F index the live save's signed bonus; other ids
/// return zero. This counts attachment positions, independently of occupancy.
s32 equipmentGetArmorAttachmentSlotCount(s32 armorItemId);

/// Initializes and draws the player-stat panel shown after an item boost.
///
/// `task->spawnArg2.pointer` must hold its task-owned live `UiObject`.
/// State zero sizes and centers it; every call draws HP, MP, experience and BP.
void itemMenuPlayerStatsPanelTask(Task* task);

/// Returns the first matching stack's quantity from an absolute row cursor.
///
/// `table` must be the range's readable backing table. On entry `*rowIndex`
/// must be within the range, or at its end. Skipped rows advance the cursor;
/// a hit leaves it at that row, and exhaustion leaves it at the end. There is
/// no lower-bound check. The result narrows the stored u16 to s16; zero also
/// means no match. The table and descriptor are borrowed without modification.
s16 inventoryFindStackQuantity(const InventoryItemRow* table, const InventoryItemRange* range, s32* rowIndex, s32 itemId);

/// Updates the Ice Bag's two-minute melting transition without returning status.
///
/// Uses the same collection-bit transition and signed 16-bit elapsed-minute
/// comparison as `inventoryMeltIceBagIfExpired`; the marker is retained.
void inventoryUpdateIceBag(void);

void func_800B8014(void);

/// Moves the item at scan slot `arg1` onto slot `arg2`, shifting the
/// occupied rows between them toward the hole left at `arg1`.
void Gp_MoveItemSlot(InventoryItemRange* arg0, s32 arg1, s32 arg2);

extern ArmorStats Gp_ModStatAttrs[];

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

/// Outcome of a battle-reward grant; a granted bonus takes precedence.
enum {
    INVENTORY_BATTLE_REWARD_NONE_GRANTED  = 0,
    INVENTORY_BATTLE_REWARD_ITEM_GRANTED  = 1,
    INVENTORY_BATTLE_REWARD_BONUS_GRANTED = 2
};

/// Grants the current area's first matching battle-reward row into a range.
///
/// Uses the live stage, area and placement variant; view does not affect the key.
/// Stage must be 1..5 with its map package's reward lists loaded. Normal/replay
/// and Scavenger use the ordinary list; Bounty and Nightmare use the other list.
/// Each nonzero slot requests one pack, subject to the item's ownership limit;
/// the bonus slot also requires an attached Medicine Wheel. Returns 0 for no grant,
/// 1 for ordinary items or 2 when the bonus slot is granted. The result records
/// attempted grants after the ownership check, so the caller must provide
/// enough writable rows/capacity for them to succeed.
/// The range descriptor is borrowed and remains unchanged.
s32 inventoryGrantBattleRewards(const InventoryItemRange* rewardRange);

/// Publishes the first placed object with a flag index in the saved stage.
///
/// Returns 1 on a match and 0 when no room table exists or the search ends.
/// Requires a valid saved stage and a matching flag index (0..0xFFFE) in its readable room
/// lists, or a table ending with `AREA_OBJECT_ROOM_LOOKUP_END`. Stored tables use
/// a different terminator, so an absent index is not a safe general query.
/// Copies the index, object kind and place state into the shared pickup outputs.
/// Item-bank kinds also publish quantity/readiness: duplicates become Ringer's
/// Solution (weapons) or Belt Pouch (other limited items); consumables use one
/// pack except object state 3, which requests the full stack. Non-item banks
/// retain the previous quantity/readiness. A failed search retains all outputs.
/// Borrows placement/stack tables without retaining a pointer or changing them.
s32 itemPickupPublishPlacedObject(s32 flagIndex);

/// Byte remap of an item id used as a sort/order key (`inventoryGetItemSortKey` /
/// `inventorySortItems`). Split by item class: 0x01–0x5F → `Gp_ItemSortKey0[id]`,
/// 0x60–0x7F → `Gp_ItemSortKey60[id-0x60]`, 0x80–0x9F → `Gp_ItemSortKey80[id-0x80]`,
/// 0xA0–0xBF → `Gp_ItemSortKeyA0[id-0xA0]`. A 0 entry (or an id outside those
/// ranges) falls back to `id + 0x100`; id 0 returns 0x1000.
extern u8 Gp_ItemSortKey0[];

extern u8 Gp_ItemSortKey60[];

extern u8 Gp_ItemSortKey80[];

extern u8 Gp_ItemSortKeyA0[];

/// Sorts the selected range by catalogue order, placing free rows last.
///
/// Exchanges complete rows as each lower key is encountered. Equal keys are
/// not exchanged directly, but the sort is not stable. Attachment positions
/// travel with their rows; saved weapon loads are indexed by item id instead.
/// The whole range must fit its table. Existing row pointers remain valid
/// addresses but may refer to different items afterwards. `unused` is ignored.
void inventorySortItems(const InventoryItemRange* range, s32 unused);

/// Adds an item for a row transfer, using the requested position when possible.
///
/// Consumables (0xA0..0xBF) use `inventoryGiveItem`'s quantity and capacity
/// rules. If the requested row is empty, their first stack moves there in full;
/// an occupied requested row stays intact and the returned stack can be elsewhere.
/// Other ids replace the requested row with quantity one, ignoring `quantity`,
/// and attempt to re-add its displaced item. A failed re-add loses that item.
/// Zero is allowed to leave an empty row. Attachments are left in their original
/// rows, including a relocated stack's vacated row; callers restore them for swaps.
/// `rowIndex` must be 0..rowCount-1, `itemId` a one-byte row id, and the range
/// must fit writable backing storage. No index check is performed. The descriptor
/// is unchanged. The result borrows its table, or is NULL if a consumable could
/// not be added; sorting or later transfers can replace its item.
InventoryItemRow* inventoryPlaceItemAtRow(const InventoryItemRange* range, s32 rowIndex, s32 itemId, s32 quantity);

/// Returns the zero-based matching weapon id from the selected table.
///
/// A zero `consumableItemId` selects every weapon row. Otherwise a weapon must
/// accept that consumable in either load and be attached to armour or currently
/// equipped. Each matching load counts separately, even on the same row.
/// Walking starts at `range->firstRow` and does not stop at `rowCount`: the
/// caller must supply a nonnegative index and enough readable rows to reach
/// `matchIndex + 1` matches.
s32 inventoryGetNthWeaponForConsumable(const InventoryItemRange* range, s32 matchIndex, s32 consumableItemId);

/// Removes an item's positive armour attachment position.
///
/// An unattached item or the equipped armour marker is unchanged. Detaching a
/// weapon also clears its removable loads unless it is the equipped weapon;
/// built-in supplies remain loaded. The row and live save must be writable.
void inventoryDetachItem(InventoryItemRow* row);

/// Quantity request that adds a consumable's maximum stack rather than one pack.
enum { INVENTORY_ADD_FULL_STACK = -2 };

/// Adds item units to the selected range and returns the affected row, or NULL.
///
/// Consumable ids 0xA0..0xBF share the first matching stack, clamped to its
/// catalogue capacity; otherwise the first free row is initialized. Any
/// negative quantity requests one pack, except `INVENTORY_ADD_FULL_STACK`,
/// which requests a full stack. Other one-byte item ids always occupy a new
/// row with quantity one. `itemId` must be 1..0xFF and the range must fit its
/// writable table. The returned
/// row borrows that table and may change item identity after sorting/transfers.
InventoryItemRow* inventoryAddItem(const InventoryItemRange* range, s32 itemId, s32 quantity);

void func_800B92CC(Task* task);

extern InventoryItemRange Gp_DefaultScan;

#endif // GAMEPLAY_PRIVATE_ITEMS_H
