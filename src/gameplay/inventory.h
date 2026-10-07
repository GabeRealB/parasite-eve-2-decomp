#ifndef GAMEPLAY_PRIVATE_INVENTORY_H
#define GAMEPLAY_PRIVATE_INVENTORY_H

#include "common.h"

#include "main/mc.h"
#include "main/mc_types.h"

/// Special features granted while an armor row is equipped.
///
/// Each bit is independent. The specifications panel walks bits 0 through 12
/// and draws at most two names. Bits 10 and 11 are part of that walk, have
/// no name, and are unset in the catalogue.
enum {
    ARMOR_FEATURE_MOTION_DETECTOR    = 0x0001,
    ARMOR_FEATURE_RESIST_POISON      = 0x0002,
    ARMOR_FEATURE_MP_GENERATION      = 0x0004,
    ARMOR_FEATURE_QUICK_FIRE         = 0x0008,
    ARMOR_FEATURE_RESIST_IMPACT      = 0x0010,
    ARMOR_FEATURE_RESIST_SILENCE     = 0x0020,
    ARMOR_FEATURE_MEDICAL_INSPECTION = 0x0040,
    ARMOR_FEATURE_MP_RECOVERY        = 0x0080,
    ARMOR_FEATURE_HP_RECOVERY        = 0x0100,
    ARMOR_FEATURE_RESIST_CONFUSION   = 0x0200,
    ARMOR_FEATURE_RESIST_PARALYSIS   = 0x1000
};

/// Most attachment slots one armor item can provide.
enum {
    ARMOR_ATTACHMENT_SLOT_MAX = 10
};

/// Stats for one armor item, ids 0x60–0x7F.
///
/// `Gp_ModStatAttrs` holds one row per id, indexed by the id minus 0x60.
/// While that armor is equipped, `hpBonus` is added to max HP and `mpBonus`
/// to max MP. `baseAttachmentSlots` plus the save's per-armor level bonus,
/// clamped to `ARMOR_ATTACHMENT_SLOT_MAX`, is how many attachment slots it has.
typedef struct {
    s32 features;            // ARMOR_FEATURE_* bits granted while equipped.
    u8  hpBonus;             // Added to max HP while this armor is equipped.
    u8  baseAttachmentSlots; // Base attachment slots, before the save's level bonus.
    u8  mpBonus;             // Added to max MP while this armor is equipped.
    u8  field_7;             // Role unproven. Every catalogue row stores 0.
} ArmorStats;
STATIC_ASSERT_SIZEOF(ArmorStats, 0x8);

/// Returns the supplied inventory-row pointer without accessing the row.
///
/// The result aliases `row`; ownership and lifetime stay with its table.
/// Sorting or transferring items can replace the contents at that address.
static inline InventoryItemRow* _inventoryRowRef(InventoryItemRow* row)
{
    // Keeping the indexed address in the argument preserves offset-first addition.
    return row;
}

/// Row `index` elements after `rows`.
/// The result borrows `rows`; `index` must name a row in that table.
#define gpItemRowAt(rows, index) _inventoryRowRef(&(rows)[index])

/// Borrows the live save's writable load record for a weapon item id.
///
/// `weaponItemId` must be 0x80..0x9F; there is no bounds check or none sentinel.
/// The record is indexed by item id, independently of inventory position or
/// which weapon is equipped. Loading or resetting the live save can replace
/// its contents. This is the inline form of `equipmentGetWeaponLoad`.
static inline EquipmentWeaponLoad* _equipmentGetWeaponLoad(s32 weaponItemId)
{
    return &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
}

/// Borrows the zero-based armor-attachment candidate in a range, or NULL.
///
/// The range must fit its backing table; choiceIndex is nonnegative. Empty
/// rows, items with ITEM_FLAG_NO_ATTACHMENT and the equipped weapon are
/// excluded. Already attached items remain candidates. unused is ignored.
/// Sorting, transfers or resetting storage can replace the returned contents.
InventoryItemRow* inventoryFindNthAttachmentCandidate(const InventoryItemRange* range, s32 choiceIndex, s32 unused);

#endif // GAMEPLAY_PRIVATE_INVENTORY_H
