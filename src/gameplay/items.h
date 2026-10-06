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
extern EquipmentWeaponLoadOptionsTable Gp_RelatedQty1;

extern EquipmentWeaponSupply Gp_ItemMaps[];

extern InventoryItemRow* Gp_ItemTable1;

extern const char Gp_StrNotice2[8];

s32 Gp_EquipRelatedBank(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

struct UiObject;

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

/// Confirmation UI for raising `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemLevelBonus` of the equipped
/// 0x60–0x7F item (`gPlayerStatus.armor`). If the clamped level is
/// already 10, `Gp_NoticePanelTask` is shown with spawnArg1 0x1A. Otherwise
/// consumes `Gp_SelItemRec` and draws "More <item> attachments available."
void Gp_UiBoostAttach(struct UiObject* arg0, Task* arg1);

void Gp_UiBoostMp(struct UiObject* arg0, Task* arg1);

/// HP counterpart of `Gp_UiBoostMp`: adds 5 to `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.hpBonus`
/// (clamped below 250), recomputes max HP (same body as `Gp_RecalcMaxHp`),
/// heals current HP to that max, then consumes `Gp_SelItemRec` and spawns
/// `Gp_BoostPanelDesc`. `Gp_NoticePanelTask` is called with `spawnArg1` forced to 0x1C.
void Gp_UiBoostHp(struct UiObject* arg0, Task* arg1);

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

s32 Gp_CountEquippedRelated(InventoryItemRange* arg0, s32 arg1);

/// Clears removable weapon loads while preserving any built-in Battery or Fuel.
///
/// Weapon ids outside 0x80..0x9F are ignored. Cleared loads lose their item id
/// and quantity; an unavailable secondary slot keeps its marker. Built-in
/// supply selection and charge, inventory quantities and the saved record's
/// unproven word are left intact. Records remain owned by the live save.
void equipmentClearRemovableLoads(s32 weaponItemId);

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

s32 Gp_FillRelated(s32 arg0, s32 arg1);

s32 Gp_UnequipRelated(s32 arg0, s32 arg1);

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

void Gp_InitModeEquip(void);

/// Clears all 128 collection bits in the live save.
///
/// Key-item possession and item-related event bits are reset together. The
/// separate Ice Bag play-time marker and inventory rows are left intact.
void inventoryClearCollectedBits(void);

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

s32 Gp_LookupBit2Item(s32 arg0);

/// Byte remap of an item id used as a sort/order key (`Gp_ItemSortKey` /
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

/// Writes item `arg2` into scan slot `arg1`. Ids `0xA0..0xBF` are added with
/// `inventoryGiveItem` first, then an existing stack is moved onto the slot when
/// it is empty. Other ids overwrite the slot (re-adding the previous item).
InventoryItemRow* Gp_SetScanItem(InventoryItemRange* arg0, s32 arg1, s32 arg2, s32 arg3);

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
