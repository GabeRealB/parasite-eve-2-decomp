#include "gameplay/starter_inventory.h"

#include "types.h"

#include "area_entry.h"
#include "attachments.h"
#include "gameplay/inventory.h"
#include "gameplay/items.h"
#include "items.h"

#include "main/mc.h"
#include "main/session.h"
#include "main/task_types.h"
#include "main/wipsys.h"

/// Catalogue ids, attachment positions and container indices used by the field-loadout reset.
enum {
    INVENTORY_STARTER_ITEM_RECOVERY_2          = 0x02,
    INVENTORY_STARTER_ITEM_BELT_POUCH          = 0x0D,
    INVENTORY_STARTER_ITEM_PROTEIN_CAPSULE     = 0x3C,
    INVENTORY_STARTER_ITEM_RINGERS_SOLUTION    = 0x3D,
    INVENTORY_STARTER_ITEM_GPS                 = 0x40,
    INVENTORY_STARTER_ITEM_LEATHER_JACKET      = 0x60,
    INVENTORY_STARTER_ITEM_ASSAULT_SUIT        = 0x63,
    INVENTORY_STARTER_ITEM_TACTICAL_VEST       = 0x65,
    INVENTORY_STARTER_ITEM_M93R                = 0x81,
    INVENTORY_STARTER_ITEM_GRENADE_PISTOL      = 0x8A,
    INVENTORY_STARTER_ITEM_TONFA               = 0x92,
    INVENTORY_STARTER_ITEM_M4A1_GRENADE        = 0x9A,
    INVENTORY_STARTER_ITEM_MP5A5_FIRST         = 0x9D,
    INVENTORY_STARTER_ITEM_MP5A5_COUNT         = 3,
    INVENTORY_STARTER_ITEM_9MM_PB              = 0xA0,
    INVENTORY_STARTER_AMMO_ROUNDS              = 100,
    INVENTORY_STARTER_CHARACTER_VARIANT        = 3,
    INVENTORY_STARTER_GPS_ATTACHMENT_SLOT      = 1,
    INVENTORY_STARTER_RECOVERY_ATTACHMENT_SLOT = 2,
    INVENTORY_STARTER_M93R_ATTACHMENT_SLOT     = 3,
    INVENTORY_STARTER_STORED_RANGE_INDEX       = 3,
    INVENTORY_STARTER_TEMP_RANGE_A_INDEX       = 1,
    INVENTORY_STARTER_TEMP_RANGE_B_INDEX       = 2
};

extern InventoryItemRange D_8010D524;

extern InventoryItemRange D_8010D528;

extern InventoryItemRange D_8010D52C;

extern InventoryItemRange D_8010D530;

extern InventoryItemRange D_8010D534;

extern InventoryItemRange D_8010D538;

extern InventoryItemRange D_8010D53C;

extern InventoryItemRange D_8010D540;

extern InventoryItemRange D_8010D544;

extern InventoryItemRange D_8010D548;

extern InventoryItemRange D_8010D54C;

InventoryItemRange  Gp_DefaultScan  = { 20, 10, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D524      = { 30, 10, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D528      = { 40, 10, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D52C      = { 50, 30, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D530      = { 80, 10, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D534      = { 90, 20, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D538      = { 110, 10, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D53C      = { 120, 10, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D540      = { 130, 20, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D544      = { 150, 10, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D548      = { 160, 10, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange  D_8010D54C      = { 170, 10, INVENTORY_ITEM_TABLE_SAVED, 0 };
InventoryItemRange* Gp_ScanPtrs[12] = { &D_8010CA2C, &D_8010D524, &D_8010D528, &D_8010D52C, &D_8010D530, &D_8010D534, &D_8010D538, &D_8010D53C, &D_8010D540, &D_8010D544, &D_8010D548, &D_8010D54C };

/// Stores carried extras and converts selected training equipment before the reset.
///
/// The borrowed ranges must be disjoint and backed by live writable tables.
/// Transfers can fail without stopping the pass; the caller still clears carried rows.
static inline void _inventoryStoreStarterExtras(const InventoryItemRange* carriedRange, const InventoryItemRange* storedRange, InventoryItemRow* itemTable)
{
    InventoryItemRow* carriedRow;
    s32               carriedRowIndex;
    u8                itemId;

    carriedRow      = &itemTable[carriedRange->firstRow];
    carriedRowIndex = 0;
    if (carriedRange->rowCount != 0) {
        do {
            itemId = carriedRow->itemId;
            if (itemId != INVENTORY_ITEM_NONE) {
                if ((u8)(itemId + (0x100 - INVENTORY_STARTER_ITEM_MP5A5_FIRST)) < INVENTORY_STARTER_ITEM_MP5A5_COUNT) {
                    inventoryGiveItem(storedRange, INVENTORY_STARTER_ITEM_RINGERS_SOLUTION, 1);
                } else if (itemId == INVENTORY_STARTER_ITEM_GRENADE_PISTOL) {
                    inventoryGiveItem(storedRange, INVENTORY_STARTER_ITEM_PROTEIN_CAPSULE, 1);
                } else if (itemId == INVENTORY_STARTER_ITEM_TACTICAL_VEST) {
                    inventoryGiveItem(storedRange, INVENTORY_STARTER_ITEM_BELT_POUCH, 1);
                } else if ((itemId != INVENTORY_STARTER_ITEM_M93R) && (itemId != INVENTORY_STARTER_ITEM_9MM_PB) && (itemId != INVENTORY_STARTER_ITEM_LEATHER_JACKET) &&
                           (itemId != INVENTORY_STARTER_ITEM_GPS) && (itemId != INVENTORY_STARTER_ITEM_TONFA)) {
                    inventoryGiveItem(storedRange, carriedRow->itemId, carriedRow->qty);
                }
            }
            carriedRowIndex++;
            carriedRow++;
        } while (carriedRowIndex < carriedRange->rowCount);
    }
}

void inventoryInitializeStarterLoadout(void)
{
    InventoryItemRange*  carriedRange;
    McSaveData*          save;
    PlayerStatus*        playerStatus;
    PlayerStatus*        restoredStatus;
    InventoryItemRow*    itemTable;
    InventoryItemRow*    attachedRow;
    InventoryItemRange** containerRanges;
    InventoryItemRange*  storedRange;
    EquipmentWeaponLoad* weaponLoad;
    s32                  weaponIndex;
    u16                  maximumHp;
    u16                  maximumMp;
    s32                  hadArmoryCardkey;
    s32                  hadMendelJournal;

    carriedRange                                                                                              = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    save                                                                                                      = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.itemLevelBonus[INVENTORY_STARTER_ITEM_TACTICAL_VEST - INVENTORY_STARTER_ITEM_LEATHER_JACKET]  = 0;
    save->state.itemLevelBonus[INVENTORY_STARTER_ITEM_LEATHER_JACKET - INVENTORY_STARTER_ITEM_LEATHER_JACKET] = 0;
    playerStatus                                                                                              = &gPlayerStatus;
    switch (carriedRange->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            itemTable = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            itemTable = Gp_ItemTable1;
            break;
        default:
            itemTable = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    storedRange = Gp_ScanPtrs[INVENTORY_STARTER_STORED_RANGE_INDEX];
    _inventoryStoreStarterExtras(carriedRange, storedRange, itemTable);
    // Rebuild the carried loadout and weapon supplies for field deployment.
    inventoryClearItems(carriedRange);
    containerRanges = Gp_ScanPtrs;
    inventoryClearItems(containerRanges[INVENTORY_STARTER_TEMP_RANGE_A_INDEX]);
    inventoryClearItems(containerRanges[INVENTORY_STARTER_TEMP_RANGE_B_INDEX]);
    weaponLoad = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems;
    for (weaponIndex = 0; weaponIndex < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems); weaponIndex++) {
        weaponLoad->primaryItemId   = INVENTORY_ITEM_NONE;
        weaponLoad->primaryQty      = 0;
        weaponLoad->secondaryItemId = EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE;
        weaponLoad->secondaryQty    = 0;
        // The M4A1 grenade launcher has a reloadable secondary slot.
        if (weaponIndex == INVENTORY_STARTER_ITEM_M4A1_GRENADE - EQUIPMENT_WEAPON_ITEM_FIRST) {
            weaponLoad->secondaryItemId = INVENTORY_ITEM_NONE;
            weaponLoad->secondaryQty    = 0;
        }
        weaponLoad->field_4 = 0;
        weaponLoad++;
    }
    equipmentInitializeWeaponSupplies();
    inventoryGiveItem(carriedRange, INVENTORY_STARTER_ITEM_ASSAULT_SUIT, 1);
    gGameSession->loadedCharacterId = GAME_SESSION_CHARACTER_NOT_LOADED;
    playerStatus->weapon            = PLAYER_STATUS_EQUIPMENT_NONE;
    playerStatus->resourceVariant   = INVENTORY_STARTER_CHARACTER_VARIANT;
    equipmentEquipCarriedArmor(INVENTORY_STARTER_ITEM_ASSAULT_SUIT);
    attachedRow             = inventoryGiveItem(carriedRange, INVENTORY_STARTER_ITEM_GPS, 1);
    attachedRow->attachSlot = INVENTORY_STARTER_GPS_ATTACHMENT_SLOT;
    attachedRow             = inventoryGiveItem(carriedRange, INVENTORY_STARTER_ITEM_RECOVERY_2, 1);
    attachedRow->attachSlot = INVENTORY_STARTER_RECOVERY_ATTACHMENT_SLOT;
    attachedRow             = inventoryGiveItem(carriedRange, INVENTORY_STARTER_ITEM_M93R, 1);
    attachedRow->attachSlot = INVENTORY_STARTER_M93R_ATTACHMENT_SLOT;
    inventoryGiveItem(carriedRange, INVENTORY_STARTER_ITEM_9MM_PB, INVENTORY_STARTER_AMMO_ROUNDS);
    equipmentLoadWeaponConsumable(carriedRange, INVENTORY_STARTER_ITEM_M93R, INVENTORY_STARTER_ITEM_9MM_PB, EQUIPMENT_WEAPON_LOAD_TO_CAPACITY);
    inventoryGiveItem(carriedRange, INVENTORY_STARTER_ITEM_TONFA, 1);
    restoredStatus     = &gPlayerStatus;
    maximumHp          = restoredStatus->hpMax;
    maximumMp          = restoredStatus->mpMax;
    restoredStatus->hp = maximumHp;
    restoredStatus->mp = maximumMp;
    // Reset collection progress while retaining the two optional documents.
    hadArmoryCardkey = inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ARMORY_CARDKEY);
    hadMendelJournal = inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_MENDEL_JOURNAL);
    inventoryClearCollectedBits();
    if (hadArmoryCardkey != 0) {
        inventorySetCollectedBit(INVENTORY_COLLECTION_ID_ARMORY_CARDKEY);
    }
    if (hadMendelJournal != 0) {
        inventorySetCollectedBit(INVENTORY_COLLECTION_ID_MENDEL_JOURNAL);
    }
    inventorySetCollectedBit(INVENTORY_COLLECTION_ID_MIST_BADGE);
    inventorySetCollectedBit(INVENTORY_COLLECTION_ID_DRYFIELD_MAP);
    inventorySetCollectedBit(INVENTORY_COLLECTION_ID_MANUAL);
    inventorySetCollectedBit(INVENTORY_COLLECTION_ID_NMC_PHOTO);
    inventorySetCollectedBit(INVENTORY_COLLECTION_ID_MIST_SEARCH_WARRANT);
}
