#include "gameplay/items.h"

#include <psyq/memory.h>

#include "common.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "item_use.h"
#include "items.h"
#include "item_menu.h"

#include "main/mc.h"
#include "main/wipsys.h"

InventoryItemRow* gInventoryIndirectRows;

/// Weapon item ids whose configurations share an inventory ownership limit.
enum {
    INVENTORY_ITEM_P08_SNAIL_MAGAZINE     = 0x80,
    INVENTORY_ITEM_P08                    = 0x83,
    INVENTORY_ITEM_M4A1                   = 0x8F,
    INVENTORY_ITEM_M4A1_ONE_CLIP_HOLDER   = 0x93,
    INVENTORY_ITEM_M4A1_TWO_CLIP_HOLDERS  = 0x94,
    INVENTORY_ITEM_M4A1_HAMMER            = 0x98,
    INVENTORY_ITEM_M4A1_BAYONET           = 0x99,
    INVENTORY_ITEM_M4A1_GRENADE           = 0x9A,
    INVENTORY_ITEM_M4A1_PYKE              = 0x9B,
    INVENTORY_ITEM_M4A1_JAVELIN           = 0x9C,
    INVENTORY_ITEM_MP5A5                  = 0x9D,
    INVENTORY_ITEM_MP5A5_ONE_CLIP_HOLDER  = 0x9E,
    INVENTORY_ITEM_MP5A5_TWO_CLIP_HOLDERS = 0x9F,
};

/// Spends one loaded weapon unit and records its use in the live save.
///
/// Callers must exclude cheat mode and supply a nonzero `loadedQuantity`.
/// Both pointers borrow writable fields of that weapon in the live save.
/// The first carried stack loses one unit if present; a missing stack does
/// not prevent consumption. `weaponUseCount` advances only below 999999;
/// counts already at or above that threshold are left intact.
static inline void _equipmentConsumeLoadedUnit(u8* loadedQuantity, s32 consumableItemId, s32* weaponUseCount)
{
    enum { EQUIPMENT_WEAPON_USE_COUNT_MAX = 999999 };

    (*loadedQuantity)--;
    inventoryConsumeFirstStack(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, consumableItemId, 1);
    if (*weaponUseCount < EQUIPMENT_WEAPON_USE_COUNT_MAX) {
        (*weaponUseCount)++;
    }
}

/// Recomputes maximum HP, preserving unsigned accumulation and the signed cap.
///
/// Mode must be 0..3 and armor 0..32. Current HP only clamps downward.
static inline void _equipmentRecalculateMaxHp(PlayerStatus* status, const PlayerModeBaseStats* modeStats, const McSaveData* save)
{
    u16 maximumHp;

    maximumHp     = modeStats[save->state.gameMode].baseHp.hp;
    status->hpMax = maximumHp;
    maximumHp    += save->state.hpBonus;
    status->hpMax = maximumHp;
    if (status->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
        maximumHp    += gEquipmentArmorStats[status->armor - 1].hpBonus;
        status->hpMax = maximumHp;
    }
    if (status->hpMax >= PLAYER_STATUS_STAT_MAX + 1) {
        status->hpMax = PLAYER_STATUS_STAT_MAX;
    }
    if (status->hp > status->hpMax) {
        status->hp = status->hpMax;
    }
}

/// Returns catalog capacity for one weapon load, independent of possession.
///
/// Ids outside 0x80..0x9F return zero; zero selection is primary, nonzero secondary.
/// This is the inline form of `equipmentGetWeaponLoadCapacity`.
static inline s32 _equipmentGetWeaponLoadCapacity(s32 weaponItemId, s32 loadSelection)
{
    s32 weaponIndex;
    s32 capacity;

    weaponIndex = weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST;
    capacity    = 0;
    if ((u32)weaponIndex < ARRAY_SIZE(gEquipmentWeaponPrimaryLoadOptions.rows)) {
        if (loadSelection == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
            capacity = gEquipmentWeaponPrimaryLoadOptions.rows[weaponIndex].capacity;
        } else {
            capacity = gEquipmentWeaponSecondaryLoadOptions.rows[weaponIndex].capacity;
        }
    }
    return capacity;
}

/// Returns row-item presence below 0xA0, or the first stack's signed quantity.
///
/// The borrowed table must back the readable range. Row-item quantity and
/// duplicate rows are ignored; a stack's u16 quantity narrows to s16.
/// Inputs are unchanged and no pointer is retained.
static inline s16 _inventoryGetItemPresenceOrStackQuantity(const InventoryItemRow* table, const InventoryItemRange* range, s32 itemId)
{
    s32 stackRowIndex;
    s32 present;
    s32 rowIndex;

    present = 0;
    if (itemId >= INVENTORY_CONSUMABLE_ITEM_FIRST) {
        stackRowIndex = range->firstRow;
        return inventoryFindStackQuantity(table, range, &stackRowIndex, itemId);
    }
    for (rowIndex = range->firstRow; rowIndex < range->firstRow + range->rowCount; rowIndex++) {
        if (table[rowIndex].itemId == itemId) {
            present = 1;
            break;
        }
    }
    return present;
}

EquipmentWeaponLoadOptionsTable gEquipmentWeaponSecondaryLoadOptions                    = { { { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 50, { 181, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 40, { 187, 0, 0 } }, { 0, { 0, 0, 0 } }, { 1, { 169, 170, 171 } }, { 30, { 189, 0, 0 } }, { 60, { 190, 0, 0 } }, { 50, { 181, 0, 0 } }, { 50, { 181, 0, 0 } }, { 50, { 181, 0, 0 } } } };
EquipmentWeaponSupply           gEquipmentWeaponSupplies[EQUIPMENT_WEAPON_SUPPLY_COUNT] = {
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x84, 0xB5, 0 }, // P229, Battery
    { EQUIPMENT_WEAPON_SUPPLY_PRIMARY, 0x95, 0xB9, 0 },   // Hypervelocity, Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x98, 0xBB, 0 }, // M4A1 Hammer, Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9B, 0xBD, 0 }, // M4A1 Pyke, Fuel
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9C, 0xBE, 0 }, // M4A1 Javelin, Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9D, 0xB5, 0 }, // MP5A5, Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9E, 0xB5, 0 }, // MP5A5(+1), Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9F, 0xB5, 0 }, // MP5A5(+2), Battery
};

void equipmentInitializeWeaponSupplies(void)
{
    s32                          supplyIndex;
    const EquipmentWeaponSupply* supply;
    EquipmentWeaponLoad*         weaponLoad;
    s32                          weaponItemId;

    // Install each built-in supply at its load's capacity.
    for (supplyIndex = 0; supplyIndex < ARRAY_SIZE(gEquipmentWeaponSupplies); supplyIndex++) {
        supply       = &gEquipmentWeaponSupplies[supplyIndex];
        weaponItemId = supply->weaponItemId;
        weaponLoad   = _equipmentGetWeaponLoad(weaponItemId);
        if (supply->supplyLoad == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
            weaponLoad->primaryItemId = supply->supplyItemId;
            weaponLoad->primaryQty    = _equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
        } else {
            weaponLoad->secondaryItemId = supply->supplyItemId;
            weaponLoad->secondaryQty    = _equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
        }
    }
}

s32 equipmentConsumeWeaponLoad(s32 weaponItemId, s32 request)
{
    EquipmentWeaponLoad* weaponLoad;
    s32*                 useCount;

    weaponLoad = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
    useCount   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];

    if (request == EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY && weaponLoad->primaryItemId != INVENTORY_ITEM_NONE && weaponLoad->primaryQty != 0) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) {
            _equipmentConsumeLoadedUnit(&weaponLoad->primaryQty, weaponLoad->primaryItemId, useCount);
        }
    } else if (request == EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY && weaponLoad->secondaryItemId != INVENTORY_ITEM_NONE && weaponLoad->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE &&
               weaponLoad->secondaryQty != 0) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) {
            _equipmentConsumeLoadedUnit(&weaponLoad->secondaryQty, weaponLoad->secondaryItemId, useCount);
        }
    }

    if (!(request & EQUIPMENT_WEAPON_LOAD_SECONDARY_MASK)) {
        return weaponLoad->primaryQty;
    }
    return weaponLoad->secondaryQty;
}

s32 equipmentLoadCarriedWeaponConsumable(s32 loadSelection, s32 weaponItemId, s32 consumableItemId, s32 requestedQuantity)
{
    s32                               stackRowIndex;
    const InventoryItemRow*           inventoryRows;
    const InventoryItemRange*         range;
    EquipmentWeaponLoad*              weaponLoad;
    const EquipmentWeaponLoadOptions* loadOptions;
    s32                               capacity;
    s32                               availableQuantity;
    s32                               choiceIndex;

    range         = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    inventoryRows = inventoryGetRangeTable(range);
    if ((u32)(weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST) >= ARRAY_SIZE(gEquipmentWeaponPrimaryLoadOptions.rows)) {
        return EQUIPMENT_WEAPON_LOAD_FAILED;
    }
    if (_inventoryGetItemPresenceOrStackQuantity(inventoryRows, range, weaponItemId) <= 0) {
        return EQUIPMENT_WEAPON_LOAD_FAILED;
    }
    if (loadSelection == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
        loadOptions = &gEquipmentWeaponPrimaryLoadOptions.rows[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
        capacity    = _equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
    } else {
        loadOptions = &gEquipmentWeaponSecondaryLoadOptions.rows[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
        capacity    = _equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
    }
    for (choiceIndex = 0; choiceIndex < ARRAY_SIZE(loadOptions->acceptedItemIds); choiceIndex++) {
        if (loadOptions->acceptedItemIds[choiceIndex] == consumableItemId) {
            break;
        }
    }
    if (choiceIndex == ARRAY_SIZE(loadOptions->acceptedItemIds)) {
        return EQUIPMENT_WEAPON_LOAD_FAILED;
    }
    if (requestedQuantity < 0) {
        requestedQuantity = capacity;
    }
    if (capacity < requestedQuantity) {
        requestedQuantity = capacity;
    }
    stackRowIndex     = range->firstRow;
    weaponLoad        = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
    availableQuantity = inventoryFindStackQuantity(inventoryRows, range, &stackRowIndex, consumableItemId);
    // Loaded units remain in inventory totals; reclaim this weapon's eligible load below.
    availableQuantity -= equipmentGetLoadedConsumableQuantity(range, consumableItemId);
    if (loadSelection == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
        if (weaponLoad->primaryItemId == consumableItemId) {
            availableQuantity += weaponLoad->primaryQty;
        }
    } else if (weaponLoad->secondaryItemId == consumableItemId) {
        availableQuantity += weaponLoad->secondaryQty;
    }
    if (availableQuantity <= 0) {
        return EQUIPMENT_WEAPON_LOAD_FAILED;
    }
    if (requestedQuantity != 0) {
        if (availableQuantity < requestedQuantity) {
            requestedQuantity = availableQuantity;
        }
        if (loadSelection == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
            weaponLoad->primaryItemId = consumableItemId;
            weaponLoad->primaryQty    = requestedQuantity;
        } else if (weaponLoad->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
            weaponLoad->secondaryItemId = consumableItemId;
            weaponLoad->secondaryQty    = requestedQuantity;
        } else {
            requestedQuantity = EQUIPMENT_WEAPON_LOAD_FAILED;
        }
        return requestedQuantity;
    }
    return 0;
}

s32 equipmentLoadWeaponConsumable(const InventoryItemRange* range, s32 weaponItemId, s32 consumableItemId, s32 requestedQuantity)
{
    s32                               stackRowIndex;
    const InventoryItemRow*           inventoryRows;
    EquipmentWeaponLoad*              weaponLoad;
    const EquipmentWeaponLoadOptions* loadOptions;
    s32                               capacity;
    s32                               availableQuantity;
    s32                               loadSelection;
    s32                               choiceIndex;
    s32                               stockLimited;
    s32                               loadedQuantity;

    inventoryRows = inventoryGetRangeTable(range);
    loadSelection = EQUIPMENT_WEAPON_SUPPLY_PRIMARY;
    if ((u32)(consumableItemId - INVENTORY_CONSUMABLE_ITEM_FIRST) >= INVENTORY_CONSUMABLE_ITEM_COUNT || (u32)(weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST) >= ARRAY_SIZE(gEquipmentWeaponPrimaryLoadOptions.rows)) {
        return EQUIPMENT_WEAPON_LOAD_FAILED;
    }
    if (_inventoryGetItemPresenceOrStackQuantity(inventoryRows, range, weaponItemId) <= 0) {
        return EQUIPMENT_WEAPON_LOAD_FAILED;
    }
    loadOptions = &gEquipmentWeaponPrimaryLoadOptions.rows[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
    capacity    = _equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
    for (choiceIndex = 0; choiceIndex < ARRAY_SIZE(loadOptions->acceptedItemIds); choiceIndex++) {
        if (loadOptions->acceptedItemIds[choiceIndex] == consumableItemId) {
            break;
        }
    }
    if (choiceIndex == ARRAY_SIZE(loadOptions->acceptedItemIds)) {
        loadSelection = EQUIPMENT_WEAPON_SUPPLY_SECONDARY;
        loadOptions   = &gEquipmentWeaponSecondaryLoadOptions.rows[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
        capacity      = _equipmentGetWeaponLoadCapacity(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
        for (choiceIndex = 0; choiceIndex < ARRAY_SIZE(loadOptions->acceptedItemIds); choiceIndex++) {
            if (loadOptions->acceptedItemIds[choiceIndex] == consumableItemId) {
                break;
            }
        }
        if (choiceIndex == ARRAY_SIZE(loadOptions->acceptedItemIds)) {
            return EQUIPMENT_WEAPON_LOAD_FAILED;
        }
    }
    if (requestedQuantity < 0) {
        requestedQuantity = capacity;
    }
    if (capacity < requestedQuantity) {
        requestedQuantity = capacity;
    }
    stackRowIndex     = range->firstRow;
    weaponLoad        = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[weaponItemId - EQUIPMENT_WEAPON_ITEM_FIRST];
    availableQuantity = inventoryFindStackQuantity(inventoryRows, range, &stackRowIndex, consumableItemId);
    // Loaded units remain in inventory totals; reclaim this weapon's eligible load below.
    availableQuantity -= equipmentGetLoadedConsumableQuantity(range, consumableItemId);
    if (weaponLoad->primaryItemId == consumableItemId) {
        availableQuantity += weaponLoad->primaryQty;
    }
    if (weaponLoad->secondaryItemId == consumableItemId) {
        availableQuantity += weaponLoad->secondaryQty;
    }
    if (availableQuantity <= 0) {
        return EQUIPMENT_WEAPON_LOAD_FAILED;
    }
    // Compute the stock clamp before the check-only branch.
    stockLimited = availableQuantity < requestedQuantity;
    if (requestedQuantity != 0) {
        loadedQuantity = stockLimited ? availableQuantity : requestedQuantity;
        if (loadSelection == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
            weaponLoad->primaryItemId = consumableItemId;
            weaponLoad->primaryQty    = loadedQuantity;
        } else if (weaponLoad->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
            weaponLoad->secondaryItemId = consumableItemId;
            weaponLoad->secondaryQty    = loadedQuantity;
        }
    } else {
        loadedQuantity = 0;
    }
    // An unavailable secondary still reports this quantity without storing it.
    if (loadedQuantity > 0) {
        itemSetIdentified(consumableItemId, 1);
    }
    return loadedQuantity;
}

/// M4A1 configurations sharing a single-copy ownership limit across saved containers.
static u8 _gInventoryM4a1LimitFamilyItemIds[] = {
    INVENTORY_ITEM_M4A1,
    INVENTORY_ITEM_M4A1_ONE_CLIP_HOLDER,
    INVENTORY_ITEM_M4A1_TWO_CLIP_HOLDERS,
    INVENTORY_ITEM_M4A1_HAMMER,
    INVENTORY_ITEM_M4A1_BAYONET,
    INVENTORY_ITEM_M4A1_GRENADE,
    INVENTORY_ITEM_M4A1_PYKE,
    INVENTORY_ITEM_M4A1_JAVELIN,
};

/// P08 and P08 snail-magazine configurations sharing a single-copy ownership limit.
static u8 _gInventoryP08LimitFamilyItemIds[] = { INVENTORY_ITEM_P08_SNAIL_MAGAZINE, INVENTORY_ITEM_P08 };

/// MP5A5 clip-holder configurations sharing a single-copy ownership limit.
static u8 _gInventoryMp5a5LimitFamilyItemIds[] = { INVENTORY_ITEM_MP5A5, INVENTORY_ITEM_MP5A5_ONE_CLIP_HOLDER, INVENTORY_ITEM_MP5A5_TWO_CLIP_HOLDERS };

PlayerModeBaseStats gEquipmentModeBaseStats[4] = {
    { { 100 }, 30 }, // Normal/replay
    { { 100 }, 30 }, // Bounty
    { { 100 }, 10 }, // Scavenger
    { { 50 }, 30 },  // Nightmare
};

/// Counts an item's quantity in saved inventory rows 0..254.
///
/// `scan` must be a side-effect-free writable `InventoryItemRange` lvalue:
/// it is cleared, then set to the saved table, first row zero and 255 rows.
/// Saved row 255 is excluded. `id` is evaluated once after initialization;
/// it must be independent of `scan`. Returns `inventoryGetItemQuantity`'s
/// quantity, including its collection-item bit test, without retaining rows.
#define INVENTORY_COUNT_SAVED_ITEM_QUANTITY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS, inventoryGetItemQuantity(&(scan), (id)))

s32 inventoryIsItemLimitReached(s32 itemId)
{
    enum {
        INVENTORY_ITEM_SMG_CLIP_HOLDER   = 0x9,
        INVENTORY_ITEM_RIFLE_CLIP_HOLDER = 0xA,
        INVENTORY_ITEM_SNAIL_MAGAZINE    = 0xC,
        INVENTORY_ITEM_HAMMER            = 0x42,
        INVENTORY_ITEM_PYKE              = 0x43,
        INVENTORY_ITEM_JAVELIN           = 0x44,
        INVENTORY_ITEM_M203              = 0x45,
        INVENTORY_ITEM_M9                = 0x46,
        INVENTORY_SINGLE_COPY_ITEM_FIRST = 0x60,
        INVENTORY_SINGLE_COPY_ITEM_COUNT = 0x40,
        INVENTORY_CLIP_HOLDER_MAX        = 2
    };
    InventoryItemRange allSavedItems;
    s32                variantIndex;
    s32                variantItemId;

    // Weapon variants share a limit; mounted add-ons also consume their allowance.
    switch (itemId) {
        case INVENTORY_ITEM_M4A1:
        case INVENTORY_ITEM_M4A1_ONE_CLIP_HOLDER:
        case INVENTORY_ITEM_M4A1_TWO_CLIP_HOLDERS:
        case INVENTORY_ITEM_M4A1_HAMMER:
        case INVENTORY_ITEM_M4A1_BAYONET:
        case INVENTORY_ITEM_M4A1_GRENADE:
        case INVENTORY_ITEM_M4A1_PYKE:
        case INVENTORY_ITEM_M4A1_JAVELIN:
            for (variantIndex = 0; variantIndex < ARRAY_SIZE(_gInventoryM4a1LimitFamilyItemIds); variantIndex++) {
                variantItemId = _gInventoryM4a1LimitFamilyItemIds[variantIndex];
                if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, variantItemId)) {
                    return 1;
                }
            }
            return 0;

        case INVENTORY_ITEM_P08_SNAIL_MAGAZINE:
        case INVENTORY_ITEM_P08:
            for (variantIndex = 0; variantIndex < ARRAY_SIZE(_gInventoryP08LimitFamilyItemIds); variantIndex++) {
                variantItemId = _gInventoryP08LimitFamilyItemIds[variantIndex];
                if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, variantItemId)) {
                    return 1;
                }
            }
            return 0;

        case INVENTORY_ITEM_MP5A5:
        case INVENTORY_ITEM_MP5A5_ONE_CLIP_HOLDER:
        case INVENTORY_ITEM_MP5A5_TWO_CLIP_HOLDERS:
            for (variantIndex = 0; variantIndex < ARRAY_SIZE(_gInventoryMp5a5LimitFamilyItemIds); variantIndex++) {
                variantItemId = _gInventoryMp5a5LimitFamilyItemIds[variantIndex];
                if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, variantItemId)) {
                    return 1;
                }
            }
            return 0;

        case INVENTORY_ITEM_SMG_CLIP_HOLDER:
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_MP5A5_TWO_CLIP_HOLDERS)) {
                return 1;
            }
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_MP5A5_ONE_CLIP_HOLDER)) {
                if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_SMG_CLIP_HOLDER)) {
                    return 1;
                }
            }
            return INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_SMG_CLIP_HOLDER) >= INVENTORY_CLIP_HOLDER_MAX;

        case INVENTORY_ITEM_RIFLE_CLIP_HOLDER:
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_M4A1_TWO_CLIP_HOLDERS)) {
                return 1;
            }
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_M4A1_ONE_CLIP_HOLDER)) {
                if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_RIFLE_CLIP_HOLDER)) {
                    return 1;
                }
            }
            return INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_RIFLE_CLIP_HOLDER) >= INVENTORY_CLIP_HOLDER_MAX;

        case INVENTORY_ITEM_SNAIL_MAGAZINE:
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_P08_SNAIL_MAGAZINE) || INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_SNAIL_MAGAZINE)) {
                return 1;
            }
            return 0;

        case INVENTORY_ITEM_HAMMER:
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_M4A1_HAMMER) || INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_HAMMER)) {
                return 1;
            }
            return 0;

        case INVENTORY_ITEM_PYKE:
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_M4A1_PYKE) || INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_PYKE)) {
                return 1;
            }
            return 0;

        case INVENTORY_ITEM_JAVELIN:
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_M4A1_JAVELIN) || INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_JAVELIN)) {
                return 1;
            }
            return 0;

        case INVENTORY_ITEM_M203:
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_M4A1_GRENADE) || INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_M203)) {
                return 1;
            }
            return 0;

        case INVENTORY_ITEM_M9:
            if (INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_M4A1_BAYONET) || INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, INVENTORY_ITEM_M9)) {
                return 1;
            }
            return 0;

        default:
            if ((u32)(itemId - INVENTORY_SINGLE_COPY_ITEM_FIRST) < INVENTORY_SINGLE_COPY_ITEM_COUNT) {
                return INVENTORY_COUNT_SAVED_ITEM_QUANTITY(allSavedItems, itemId);
            }
            return 0;
    }
}

#undef INVENTORY_COUNT_SAVED_ITEM_QUANTITY

void equipmentRecalculateMaxMp(void)
{
    PlayerStatus*              status;
    const McSaveData*          save;
    const PlayerModeBaseStats* modeStats;
    const s8*                  spellLevels;
    s32                        maximumMp;
    s32                        spellIndex;
    s32                        levelIndex;

    status    = &gPlayerStatus;
    maximumMp = 0;
    // Preserve signed byte reads: nonpositive levels contribute no MP.
    spellLevels = (const s8*)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    for (spellIndex = 0; spellIndex < ATTACHMENT_SPELL_COUNT; spellIndex++) {
        if (*spellLevels > 0) {
            for (levelIndex = 0; levelIndex < *spellLevels; levelIndex++) {
                maximumMp += gAttachmentLevelTable.rows[spellIndex * ATTACHMENT_LEVEL_COUNT + levelIndex + 1].column.mpBonus;
            }
        }
        spellLevels++;
    }
    if (status->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
        maximumMp += gEquipmentArmorStats[status->armor - 1].mpBonus;
    }
    modeStats     = gEquipmentModeBaseStats;
    save          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    maximumMp    += modeStats[save->state.gameMode].baseMp;
    maximumMp    += save->state.mpBonus;
    status->mpMax = maximumMp;
    if ((s16)maximumMp >= PLAYER_STATUS_STAT_MAX + 1) {
        status->mpMax = PLAYER_STATUS_STAT_MAX;
    }
    if (status->mp > status->mpMax) {
        status->mp = status->mpMax;
    }
}

void equipmentEquipCarriedArmor(s32 armorItemId)
{
    enum { EQUIPMENT_ARMOR_ITEM_FIRST             = 0x60,
           EQUIPMENT_ARMOR_ITEM_COUNT_U           = 0x20U,
           INVENTORY_ITEM_IDENTIFICATION_LIMIT_U  = 0x180U,
           INVENTORY_IDENTIFICATION_BITS_PER_WORD = 32 };
    PlayerStatus*       status;
    InventoryItemRow*   inventoryRow;
    InventoryItemRow*   inventoryRows;
    InventoryItemRange* carriedRange;
    s32                 rowIndex;

    status = &gPlayerStatus;
    if ((u32)(armorItemId - EQUIPMENT_ARMOR_ITEM_FIRST) < EQUIPMENT_ARMOR_ITEM_COUNT_U) {
        if (status->armor != (armorItemId - (EQUIPMENT_ARMOR_ITEM_FIRST - 1))) {
            InventoryItemRow* armorRow;

            armorRow = inventoryFindLastCarriedItemRow(armorItemId);
            if (armorRow != NULL) {
                armorRow->attachSlot = INVENTORY_ATTACHMENT_EQUIPPED_ARMOR;
                if (status->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
                    armorRow = inventoryFindLastCarriedItemRow(status->armor + (EQUIPMENT_ARMOR_ITEM_FIRST - 1));
                    if (armorRow != NULL) {
                        armorRow->attachSlot = INVENTORY_ATTACHMENT_NONE;
                    }
                }
                status->armor = armorItemId - (EQUIPMENT_ARMOR_ITEM_FIRST - 1);

                {
                    PlayerStatus*              playerStatus;
                    McSaveData*                save;
                    const PlayerModeBaseStats* modeStats;

                    playerStatus = &gPlayerStatus;
                    modeStats    = gEquipmentModeBaseStats;
                    save         = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
                    _equipmentRecalculateMaxHp(playerStatus, modeStats, save);

                    carriedRange = &save->state.carriedItems;
                    equipmentRecalculateMaxMp();
                    switch (carriedRange->tableId) {
                        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
                            inventoryRows = gInventoryBattleRewardRows;
                            break;
                        case INVENTORY_ITEM_TABLE_INDIRECT:
                            inventoryRows = gInventoryIndirectRows;
                            break;
                        default:
                            inventoryRows = save->state.itemRows;
                            break;
                    }
                }
                // Changing armor releases its positive attachment slots and removable loads.
                rowIndex     = 0;
                inventoryRow = &inventoryRows[carriedRange->firstRow];
                if (carriedRange->rowCount != 0) {
                    do {
                        inventoryDetachItem(inventoryRow);
                        rowIndex++;
                        inventoryRow++;
                    } while (rowIndex < carriedRange->rowCount);
                }

                {
                    McSaveData* identificationSave;
                    s32         identificationWordIndex;
                    s32         identificationBit;

                    identificationWordIndex = armorItemId / INVENTORY_IDENTIFICATION_BITS_PER_WORD;
                    identificationBit       = 1 << (armorItemId % INVENTORY_IDENTIFICATION_BITS_PER_WORD);
                    if ((u32)armorItemId < INVENTORY_ITEM_IDENTIFICATION_LIMIT_U) {
                        identificationSave                                               = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
                        identificationSave->state.itemSeenBits[identificationWordIndex] |= identificationBit;
                    }
                }
            }
        }
    } else if (armorItemId == 0) {
        McSaveData*                save;
        const PlayerModeBaseStats* modeStats;

        modeStats = gEquipmentModeBaseStats;
        save      = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        _equipmentRecalculateMaxHp(status, modeStats, save);
        equipmentRecalculateMaxMp();
    } else {
        return;
    }
    gHudDisplayedVitals.hp = status->hp;
    gHudDisplayedVitals.mp = status->mp;
}

const char gItemMenuPouchNoticeLabel[8] = "Notice\0F";
