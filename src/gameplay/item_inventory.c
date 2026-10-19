#include "items.h"

#include "types.h"

#include "attachments.h"
#include "gameplay/inventory.h"
#include "item_menu.h"
#include "gameplay/items.h"
#include "gameplay/starter_inventory.h"

#include "main/mc.h"
#include "main/areas.h"
#include "main/session.h"
#include "main/wipsys.h"

/* Item table a scan window lies in. */
static inline InventoryItemRow* _gpScanTable(const InventoryItemRange* scan);

/// Saves a complete moved row and marks its former slot free, retaining the slot metadata.
///
/// `rowTable` and `index` are evaluated three times and must have no side
/// effects; `result` is a disjoint writable InventoryItemRow lvalue. Captures
/// no locals. Use as a standalone block statement inside braces.
#define INVENTORY_TAKE_MOVED_ROW(rowTable, index, result) \
    {                                                     \
        (result)                   = (rowTable)[(index)]; \
        (rowTable)[(index)].itemId = INVENTORY_ITEM_NONE; \
        (rowTable)[(index)].qty    = 0;                   \
    }

/// Unreferenced NUL-terminated text "You do not have"; its display role is unproven.
static const u8 D_80096E28[] = "You do not have";
/// Unreferenced NUL-terminated text "applicable weapon"; its display role is unproven.
static const u8 D_80096E38[] = "applicable weapon";

/// Grants one weapon, then loads its selected consumable to capacity.
///
/// itemRange must fit a live writable table; weaponItemId is 0x80..0x9F and
/// consumableItemId is a compatible id in 0xA0..0xBF. Loading uses saved weapon
/// loads and available stock already in the range; it does not grant stock.
/// Grant/load failures are deliberately ignored. Arguments must have no side
/// effects: range and weapon are evaluated twice, consumable once. Retains no pointer.
#define INVENTORY_GIVE_LOADED_WEAPON(itemRange, weaponItemId, consumableItemId)                                            \
    do {                                                                                                                   \
        inventoryGiveItem((itemRange), (weaponItemId), 1);                                                                 \
        equipmentLoadWeaponConsumable((itemRange), (weaponItemId), (consumableItemId), EQUIPMENT_WEAPON_LOAD_TO_CAPACITY); \
    } while (0)

/// Replaces carried items with the base new-game loadout and restores HP/MP.
///
/// itemRange must describe the live save's writable carried rows, with room for
/// the Leather Jacket, Tonfa Baton, GPS and 100 9mm PB rounds. playerStatus must
/// be the live player: equipping the jacket recomputes its maxima before refill.
/// GPS and ammunition take attachment positions 1/2; the baton is not equipped.
/// The GPS/ammunition grants must succeed, since their returned rows are written
/// without a null check. Arguments are side-effect-free pointers evaluated repeatedly.
/// Captures no caller locals, allocates no storage and keeps no returned pointer.
#define INVENTORY_INITIALIZE_STARTING_LOADOUT(itemRange, playerStatus)                                                                                  \
    do {                                                                                                                                                \
        enum { INVENTORY_STARTING_ITEM_LEATHER_JACKET = 0x60,                                                                                           \
               INVENTORY_STARTING_ITEM_TONFA_BATON    = 0x92,                                                                                           \
               INVENTORY_STARTING_ITEM_GPS            = 0x40,                                                                                           \
               INVENTORY_STARTING_ITEM_9MM_PB         = 0xA0,                                                                                           \
               INVENTORY_STARTING_9MM_ROUNDS          = 100,                                                                                            \
               INVENTORY_STARTING_GPS_ATTACHMENT      = 1,                                                                                              \
               INVENTORY_STARTING_AMMO_ATTACHMENT     = 2 };                                                                                                \
        inventoryClearItems((itemRange));                                                                                                               \
        inventoryGiveItem((itemRange), INVENTORY_STARTING_ITEM_LEATHER_JACKET, 1);                                                                      \
        equipmentEquipCarriedArmor(INVENTORY_STARTING_ITEM_LEATHER_JACKET);                                                                             \
        (playerStatus)->hp = (playerStatus)->hpMax;                                                                                                     \
        (playerStatus)->mp = (playerStatus)->mpMax;                                                                                                     \
        inventoryGiveItem((itemRange), INVENTORY_STARTING_ITEM_TONFA_BATON, 1);                                                                         \
        inventoryGiveItem((itemRange), INVENTORY_STARTING_ITEM_GPS, 1)->attachSlot                                = INVENTORY_STARTING_GPS_ATTACHMENT;  \
        inventoryGiveItem((itemRange), INVENTORY_STARTING_ITEM_9MM_PB, INVENTORY_STARTING_9MM_ROUNDS)->attachSlot = INVENTORY_STARTING_AMMO_ATTACHMENT; \
    } while (0)

/// Clears identification storage and identifies catalog entries without an unknown name.
///
/// Scans raw ids 0..383 after clearing all 96 stored words. Text contains three
/// NUL/newline-terminated identified fields; a following newline means there is
/// no unknown name. Requires live saved storage and terminated catalog text.
/// Raw ids 192..255 use the first 64 key-item descriptors through the contiguous
/// ordinary-table address; ids 256..383 use key rows 0..127 again. Those physical
/// aliases are retained, but their containing C declaration remains unproven.
static inline void _itemInitializeNewGameIdentification(void)
{
    enum { ITEM_IDENTIFICATION_KEY_ITEM_FIRST = 0x100,
           ITEM_IDENTIFICATION_ID_LIMIT       = 0x180,
           ITEM_IDENTIFIED_TEXT_FIELD_COUNT   = 3 };
    const ItemDesc* descriptor;
    const u8*       text;
    // Reused first for stored words, then for raw item ids; keeps the matching allocation.
    s32 identificationIndex;
    s32 fieldsRemaining;

    // Reset the whole saved storage, including words beyond the queried id range.
    for (identificationIndex = ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemSeenBits) - 1; identificationIndex >= 0; identificationIndex--) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemSeenBits[identificationIndex] = 0;
    }

    // Identify entries whose three known fields are followed by an empty unknown name.
    identificationIndex = 0;
    do {
        fieldsRemaining = ITEM_IDENTIFIED_TEXT_FIELD_COUNT;
        if (identificationIndex < ITEM_IDENTIFICATION_KEY_ITEM_FIRST) {
            descriptor = &gItemDescriptors[identificationIndex];
        } else {
            descriptor = &gItemKeyDescriptors[identificationIndex - ITEM_IDENTIFICATION_KEY_ITEM_FIRST];
        }
        text = descriptor->textFields;
        while (fieldsRemaining > 0) {
            if (*text == '\0' || *text == '\n') {
                fieldsRemaining--;
            }
            text++;
        }
        if (*text == '\n') {
            itemSetIdentified(identificationIndex, 1);
        }
        identificationIndex++;
    } while (identificationIndex < ITEM_IDENTIFICATION_ID_LIMIT);
}

/// Resets saved weapon loads and installs each weapon's built-in rechargeable supply.
///
/// Removable primary loads start empty. Only M4A1 Grenade starts with an available
/// removable secondary; every other secondary is unavailable before supplies
/// are installed at capacity. Resets all 32 live-save records for weapon item
/// IDs 0x80..0x9F, including each record's unexplained final word. Saved storage
/// and the supply/capacity tables must be live. Inventory rows remain intact.
static inline void _equipmentInitializeNewGameWeaponLoads(void)
{
    enum { EQUIPMENT_ITEM_M4A1_GRENADE = 0x9A };
    EquipmentWeaponLoad* weaponLoad;
    s32                  weaponIndex;

    weaponLoad = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems;
    for (weaponIndex = 0; weaponIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems); weaponIndex++) {
        weaponLoad->primaryItemId   = INVENTORY_ITEM_NONE;
        weaponLoad->primaryQty      = 0;
        weaponLoad->secondaryItemId = EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE;
        weaponLoad->secondaryQty    = 0;
        // The M4A1 grenade launcher has a reloadable secondary slot.
        if (weaponIndex == EQUIPMENT_ITEM_M4A1_GRENADE - EQUIPMENT_WEAPON_ITEM_FIRST) {
            weaponLoad->secondaryItemId = INVENTORY_ITEM_NONE;
            weaponLoad->secondaryQty    = 0;
        }
        weaponLoad->field_4 = 0;
        weaponLoad++;
    }
    equipmentInitializeWeaponSupplies();
}

void inventoryInitializeNewGame(void)
{
    enum {
        INVENTORY_NEW_GAME_ITEM_RECOVERY_1        = 0x01,
        INVENTORY_NEW_GAME_ITEM_RECOVERY_2        = 0x02,
        INVENTORY_NEW_GAME_ITEM_STIM              = 0x04,
        INVENTORY_NEW_GAME_ITEM_M93R              = 0x81,
        INVENTORY_NEW_GAME_ITEM_9MM_PB            = 0xA0,
        INVENTORY_NEW_GAME_ITEM_GRENADE           = 0xA9,
        INVENTORY_NEW_GAME_ITEM_BUCKSHOT          = 0xAC,
        INVENTORY_NEW_GAME_CARRIED_ROW_COUNT      = 20,
        INVENTORY_NEW_GAME_ENERGY_PAGE_COUNT      = 4,
        INVENTORY_NEW_GAME_ENERGY_COLUMN_COUNT    = 3,
        INVENTORY_NEW_GAME_STARTING_BP            = 200,
        INVENTORY_NEW_GAME_RECOVERY_ATTACHMENT    = 3,
        INVENTORY_NEW_GAME_STORED_BUCKSHOT_ROUNDS = 20,
        INVENTORY_NEW_GAME_STORED_GRENADES        = 8
    };
    InventoryItemRow*          itemRow;
    McSaveData*                save;
    InventoryItemRange*        itemRange;
    InventoryItemRange* const* containerRanges;
    PlayerStatus*              playerStatus;
    s32                        stageAreaKey;
    s32                        rowIndex;
    s32                        pageIndex;
    s32                        columnIndex;

    // Reset saved rows and identification; attachment bytes in empty rows survive.
    for (rowIndex = 0, itemRow = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows; rowIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows); rowIndex++) {
        itemRow->itemId = INVENTORY_ITEM_NONE;
        itemRow->qty    = 0;
        itemRow++;
    }
    _itemInitializeNewGameIdentification();
    // Reset collected items and every weapon load before installing built-in supplies.
    inventoryClearCollectedBits();
    _equipmentInitializeNewGameWeaponLoads();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.firstRow = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.rowCount = INVENTORY_NEW_GAME_CARRIED_ROW_COUNT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems.tableId  = INVENTORY_ITEM_TABLE_SAVED;
    // Only the four three-entry energy pages are reset; the remaining six bytes survive.
    for (pageIndex = 0; pageIndex < INVENTORY_NEW_GAME_ENERGY_PAGE_COUNT; pageIndex++) {
        for (columnIndex = 0; columnIndex < INVENTORY_NEW_GAME_ENERGY_COLUMN_COUNT; columnIndex++) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[columnIndex + pageIndex * INVENTORY_NEW_GAME_ENERGY_COLUMN_COUNT] = 0;
        }
    }
    save                        = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    itemRange                   = &save->state.carriedItems;
    save->state.attachLevels[0] = 1;
    playerStatus                = &gPlayerStatus;
    if (save->state.clearCount == 0) {
        playerStatus->bp = INVENTORY_NEW_GAME_STARTING_BP;
        INVENTORY_INITIALIZE_STARTING_LOADOUT(itemRange, playerStatus);
    } else {
        INVENTORY_INITIALIZE_STARTING_LOADOUT(itemRange, playerStatus);
    }
    INVENTORY_GIVE_LOADED_WEAPON(itemRange, INVENTORY_NEW_GAME_ITEM_M93R, INVENTORY_NEW_GAME_ITEM_9MM_PB);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_2, 1)->attachSlot = INVENTORY_NEW_GAME_RECOVERY_ATTACHMENT;
    // Seed separate container ranges; repeated grants preserve their original row behavior.
    containerRanges = gInventoryContainerRanges;
    itemRange       = containerRanges[1];
    inventoryClearItems(itemRange);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_1, 1);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_1, 1);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_STIM, 1);
    itemRange = containerRanges[2];
    inventoryClearItems(itemRange);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_1, 1);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_RECOVERY_1, 1);
    inventoryClearItems(containerRanges[3]);
    itemRange = containerRanges[4];
    inventoryClearItems(itemRange);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_9MM_PB, INVENTORY_GIVE_ONE_PACK);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_STIM, 1);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_STIM, 1);
    inventoryClearItems(containerRanges[6]);
    inventoryClearItems(containerRanges[5]);
    itemRange = containerRanges[8];
    inventoryClearItems(itemRange);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_BUCKSHOT, INVENTORY_NEW_GAME_STORED_BUCKSHOT_ROUNDS);
    inventoryGiveItem(itemRange, INVENTORY_NEW_GAME_ITEM_GRENADE, INVENTORY_NEW_GAME_STORED_GRENADES);
    inventorySetCollectedBit(INVENTORY_COLLECTION_ID_MIST_BADGE);
    stageAreaKey  = GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
    stageAreaKey &= GAME_LOCATION_STAGE_AREA_MASK;
    if (stageAreaKey == GAME_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0, 0)) {
        inventoryInitializeShootingGalleryLoadout();
        inventoryGiveItem(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, INVENTORY_NEW_GAME_ITEM_M93R, 1);
        equipmentEquipCarriedWeapon(INVENTORY_NEW_GAME_ITEM_M93R);
    }
}

#undef INVENTORY_GIVE_LOADED_WEAPON
#undef INVENTORY_INITIALIZE_STARTING_LOADOUT

/* Item table a scan window lies in. */
static inline InventoryItemRow* _gpScanTable(const InventoryItemRange* scan)
{
    InventoryItemRow* table;

    switch (scan->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            table = gInventoryBattleRewardRows;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            table = gInventoryIndirectRows;
            break;
        default:
            table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    return table;
}

void inventoryMoveItemRow(const InventoryItemRange* range, s32 sourceIndex, s32 destinationIndex)
{
    InventoryItemRow* rows;
    InventoryItemRow  movedRow;
    s32               holeIndex;

    rows = _gpScanTable(range);
    if (sourceIndex == destinationIndex) {
        return;
    }

    // Convert range-relative slots to absolute table indices.
    sourceIndex      += range->firstRow;
    destinationIndex += range->firstRow;

    if (sourceIndex < destinationIndex) {
        INVENTORY_TAKE_MOVED_ROW(rows, sourceIndex, movedRow);
        // The nearest free row bounds the occupied run that needs shifting.
        for (holeIndex = destinationIndex; sourceIndex < holeIndex; holeIndex--) {
            if (rows[holeIndex].itemId == INVENTORY_ITEM_NONE) {
                break;
            }
        }
        for (; holeIndex < destinationIndex; holeIndex++) {
            rows[holeIndex] = rows[holeIndex + 1];
        }
    } else {
        INVENTORY_TAKE_MOVED_ROW(rows, sourceIndex, movedRow);
        for (holeIndex = destinationIndex; holeIndex < sourceIndex; holeIndex++) {
            if (rows[holeIndex].itemId == INVENTORY_ITEM_NONE) {
                break;
            }
        }
        for (; destinationIndex < holeIndex; holeIndex--) {
            rows[holeIndex] = rows[holeIndex - 1];
        }
    }
    rows[destinationIndex] = movedRow;
}

#undef INVENTORY_TAKE_MOVED_ROW
