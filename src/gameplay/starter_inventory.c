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

/* Count item `id` in saved rows 0..254 through a cleared range. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS, Gp_SumScanQty(&(scan), (id)))

void Gp_InitStarterInv(void)
{
    InventoryItemRange*  scan;
    McSaveData*          save;
    PlayerStatus*        cfg;
    PlayerStatus*        cfg2;
    InventoryItemRow*    tmp;
    InventoryItemRow*    rec;
    InventoryItemRow*    added;
    InventoryItemRange** scans;
    InventoryItemRange*  dest;
    EquipmentWeaponLoad* slots;
    s32                  i;
    s32                  j;
    u8                   item;
    s32                  three;
    u16                  hp;
    u16                  mp;
    s32                  flag105;
    s32                  flag107;

    scan                          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    save                          = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.itemLevelBonus[5] = 0;
    save->state.itemLevelBonus[0] = 0;
    cfg                           = &gPlayerStatus;
    switch (scan->tableId) {
        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
            tmp = Gp_ItemTable2;
            break;
        case INVENTORY_ITEM_TABLE_INDIRECT:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.itemRows;
            break;
    }
    rec  = &tmp[scan->firstRow];
    dest = D_8010D55C;
    i    = 0;
    if (scan->rowCount != 0) {
        do {
            item = rec->itemId;
            if (item != 0) {
                if ((u8)(item + 0x63) < 3) {
                    Gp_GiveItem(dest, 0x3D, 1);
                } else if (item == 0x8A) {
                    Gp_GiveItem(dest, 0x3C, 1);
                } else if (item == 0x65) {
                    Gp_GiveItem(dest, 0xD, 1);
                } else if ((item != 0x81) && (item != 0xA0) && (item != 0x60) &&
                           (item != 0x40) && (item != 0x92)) {
                    Gp_GiveItem(dest, rec->itemId, rec->qty);
                }
            }
            i++;
            rec++;
        } while (i < scan->rowCount);
    }
    Gp_ClearScanItems(scan);
    scans = Gp_ScanPtrs;
    Gp_ClearScanItems(scans[1]);
    Gp_ClearScanItems(scans[2]);
    slots = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems;
    for (j = 0; j < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems); j++) {
        slots->primaryItemId   = INVENTORY_ITEM_NONE;
        slots->primaryQty      = 0;
        slots->secondaryItemId = EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE;
        slots->secondaryQty    = 0;
        // The M4A1 grenade launcher has a reloadable secondary slot.
        if (j == 0x9A - EQUIPMENT_WEAPON_ITEM_FIRST) {
            slots->secondaryItemId = INVENTORY_ITEM_NONE;
            slots->secondaryQty    = 0;
        }
        slots->field_4 = 0;
        slots++;
    }
    three = 3;
    Gp_ApplyItemMap();
    Gp_GiveItem(scan, 0x63, 1);
    gGameSession->loadedCharacterId = GAME_SESSION_CHARACTER_NOT_LOADED;
    cfg->weapon                     = PLAYER_STATUS_EQUIPMENT_NONE;
    cfg->resourceVariant            = three;
    Gp_EquipMod(0x63);
    added             = Gp_GiveItem(scan, 0x40, 1);
    added->attachSlot = 1;
    added             = Gp_GiveItem(scan, 2, 1);
    added->attachSlot = 2;
    added             = Gp_GiveItem(scan, 0x81, 1);
    added->attachSlot = three;
    Gp_GiveItem(scan, 0xA0, 0x64);
    Gp_EquipRelatedItem(scan, 0x81, 0xA0, -1);
    Gp_GiveItem(scan, 0x92, 1);
    cfg2     = &gPlayerStatus;
    hp       = cfg2->hpMax;
    mp       = cfg2->mpMax;
    cfg2->hp = hp;
    cfg2->mp = mp;
    flag105  = Gp_HasCollectedBit(0x105);
    flag107  = Gp_HasCollectedBit(0x107);
    Gp_ClearCollectedBits();
    if (flag105 != 0) {
        Gp_SetCollectedBit(0x105);
    }
    if (flag107 != 0) {
        Gp_SetCollectedBit(0x107);
    }
    Gp_SetCollectedBit(0x106);
    Gp_SetCollectedBit(0x10C);
    Gp_SetCollectedBit(0x10B);
    Gp_SetCollectedBit(0x10A);
    Gp_SetCollectedBit(0x109);
}
