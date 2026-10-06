#ifndef GAMEPLAY_INVENTORY_H
#define GAMEPLAY_INVENTORY_H

#include "common.h"

#include "gameplay/starter_inventory.h"

/// Which load of `EquipmentWeaponLoad` holds a weapon's built-in supply.
enum {
    EQUIPMENT_WEAPON_SUPPLY_PRIMARY   = 0, // `primaryItemId` and `primaryQty`
    EQUIPMENT_WEAPON_SUPPLY_SECONDARY = 1  // `secondaryItemId` and `secondaryQty`
};

/// Number of `Gp_ItemMaps` rows, one per weapon with a built-in supply.
enum { EQUIPMENT_WEAPON_SUPPLY_COUNT = 8 };

/// Built-in rechargeable supply for one weapon.
///
/// `Gp_ItemMaps` has one row per weapon that keeps this charge in
/// `EquipmentWeaponLoad`. The catalogue names the supplies Battery or Fuel.
/// Shops refill the load `supplyLoad` selects up to that load's capacity.
/// Clearing removable ammunition leaves the supply loaded. `supplyItemId` is
/// the item id stored in the load and drawn while the shop charges it.
typedef struct {
    u8 supplyLoad;   // Load holding the supply (0 primary, 1 secondary).
    u8 weaponItemId; // Weapon item id (0x80..0x9F).
    u8 supplyItemId; // Supply item id (Battery or Fuel, in 0xA0..0xBF).
    u8 field_3;      // Unread. Zero in every row; role unproven.
} EquipmentWeaponSupply;
STATIC_ASSERT_SIZEOF(EquipmentWeaponSupply, 0x4);

/// Capacity and consumable choices for one weapon's primary or secondary load.
///
/// The gameplay tables have one four-byte row per weapon id 0x80..0x9F,
/// indexed by item id minus `EQUIPMENT_WEAPON_ITEM_FIRST`: `Gp_RelatedQty0`
/// describes the primary load and `Gp_RelatedQty1` the secondary load in
/// `EquipmentWeaponLoad`. Quantities count rounds or rechargeable supply units.
/// All three choices are searched, including slots after an unused zero;
/// the first choice supplies the ammunition icon and shooting-gallery load.
/// Row pointers borrow the tables for the gameplay image's lifetime.
typedef struct {
    u8 capacity;           // Maximum loaded rounds or supply units (0 no capacity).
    u8 acceptedItemIds[3]; // Consumable item ids (0 unused, otherwise 0xA0..0xBF), in selection order.
} EquipmentWeaponLoadOptions;
STATIC_ASSERT_SIZEOF(EquipmentWeaponLoadOptions, 0x4);

/// Catalogue of load options for every weapon item id 0x80..0x9F.
///
/// `Gp_RelatedQty0` is the primary load and `Gp_RelatedQty1` the secondary,
/// indexed by item id minus `EQUIPMENT_WEAPON_ITEM_FIRST`. Each row is that
/// weapon's capacity and the consumables its load accepts.
typedef struct {
    EquipmentWeaponLoadOptions rows[32]; // Indexed by weapon item id minus EQUIPMENT_WEAPON_ITEM_FIRST.
} EquipmentWeaponLoadOptionsTable;
STATIC_ASSERT_SIZEOF(EquipmentWeaponLoadOptionsTable, 0x80);

/// Inventory ids whose quantities share a consumable stack within a range.
enum {
    INVENTORY_CONSUMABLE_ITEM_FIRST = 0xA0,
    INVENTORY_CONSUMABLE_ITEM_COUNT = 0x20
};

/// Pack size and stack capacity for one consumable item (ids 0xA0..0xBF).
///
/// `Gp_StackLimits` holds one row per consumable, indexed by item id minus
/// 0xA0. A pack is the quantity one pickup, one shop purchase or a default
/// grant adds; the capacity bounds the quantity of the item's single stack
/// row in an item table. Row pointers borrow the table for the gameplay
/// image's lifetime.
typedef struct {
    u8  packQty; // Quantity in one pack: a pickup, a shop purchase, or a grant without an explicit count.
    u8  field_1; // Zero in every row and never read; role unproven.
    u16 maxHeld; // Largest quantity the item's stack row may hold.
} InventoryConsumableStack;
STATIC_ASSERT_SIZEOF(InventoryConsumableStack, 0x4);

/// Starting maximum HP for one game mode.
///
/// Each `PlayerModeBaseStats` carries one in `baseHp`. The count occupies the low half, and
/// the mode table stores zero in the high half, so the word view is the same
/// count. Maximum-HP recalculation reads `hp` and copies it into
/// `PlayerStatus.hpMax` before the saved HP bonus and armour. The
/// shooting-gallery status panel reads `hpWord` and prints that count. The
/// mode table's initializer writes `hp`, so that member stays first.
typedef union {
    u16 hp;     // Low half copied into the player's maximum HP.
    s32 hpWord; // All four bytes, as the status panel's HP figure.
} PlayerModeBaseHp;
STATIC_ASSERT_SIZEOF(PlayerModeBaseHp, 0x4);

/// Starting maximum HP and starting MP for one game mode.
///
/// `Gp_StatRows` holds one row for each save game mode (0 normal/replay, 1 Bounty,
/// 2 Scavenger, 3 Nightmare). Maximum HP begins at `baseHp`, before the saved HP
/// bonus and armour. Maximum MP adds `baseMp` after Parasite Energy levels and
/// armour, and before the saved MP bonus. The shooting-gallery status panel
/// prints both figures.
typedef struct {
    PlayerModeBaseHp baseHp; // Starting maximum HP.
    s32              baseMp; // Starting MP.
} PlayerModeBaseStats;
STATIC_ASSERT_SIZEOF(PlayerModeBaseStats, 0x8);

/// Low-nibble item subtypes used by inventory icons and ammunition labels.
///
/// The subtype's meaning depends on the catalogue category: medicine uses 1,
/// while ammunition uses the caliber or load kind below.
enum {
    ITEM_SUBTYPE_MASK     = 0x0F,
    ITEM_SUBTYPE_MEDICINE = 1,
    ITEM_AMMO_9MM         = 1,
    ITEM_AMMO_44_MAGNUM   = 3,
    ITEM_AMMO_40MM        = 4,
    ITEM_AMMO_12_GAUGE    = 5,
    ITEM_AMMO_556MM       = 6,
    ITEM_AMMO_BATTERY     = 8
};

/// Restrictions in `ItemDesc::flags`; these masks leave the stored byte intact.
enum {
    ITEM_FLAG_NO_DISCARD    = 0x01, // Also blocks transfers in the battle-loot menu.
    ITEM_FLAG_NO_ATTACHMENT = 0x04  // Cannot occupy an armor attachment slot.
};

/// Catalogue properties and identified/unidentified text for an inventory item.
///
/// `Gp_ItemDescs` and `Gp_KeyItemDescs` use this same row layout. The high
/// nibble of `classification` groups entries (0 other, 1 armor, 2 weapon,
/// 3 ammunition, 4 key item); the low nibble is a category-specific subtype.
/// `textFields` borrows text for the gameplay image's lifetime: an identified
/// name and two description lines, followed by their unidentified counterparts.
/// Fields end at NUL, newline or a `\n` / `\N` escape. An empty unidentified
/// name makes the item identified from the start.
typedef struct {
    u16       price;          // Purchase price in BP; replay-bonus credit is half.
    u8        classification; // Packed catalogue category and subtype.
    u8        flags;          // ITEM_FLAG_* restrictions; the role of 0x08 is unproven.
    const u8* textFields;     // Six delimited text fields, owned by the gameplay image.
} ItemDesc;
STATIC_ASSERT_SIZEOF(ItemDesc, 0x8);

/// Scan dest used while `Gp_InitStarterInv` copies the current inventory out.
#define D_8010D55C Gp_ScanPtrs[3]

#endif // GAMEPLAY_INVENTORY_H
