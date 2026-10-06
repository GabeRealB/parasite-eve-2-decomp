#include "gameplay/items.h"

#include <psyq/memory.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "item_use.h"
#include "items.h"

#include "main/mc.h"
#include "main/wipsys.h"

InventoryItemRow* Gp_ItemTable1;

extern u8 D_8010D318[8];

extern u8 D_8010D320[2];

extern u8 D_8010D324[3];

static inline s32 _gpRelatedQty(s32 item, s32 bank);

static inline s16 _gpScanHeldQty(InventoryItemRow* table, InventoryItemRange* scan, s32 item);

static inline s32 _gpRelatedQty(s32 item, s32 bank)
{
    s32 ret;

    item -= EQUIPMENT_WEAPON_ITEM_FIRST;
    ret   = 0;
    if ((u32)item < ARRAY_SIZE(Gp_RelatedQty0.rows)) {
        if (bank == 0) {
            ret = Gp_RelatedQty0.rows[item].capacity;
        } else {
            ret = Gp_RelatedQty1.rows[item].capacity;
        }
    }
    return ret;
}
static inline s16 _gpScanHeldQty(InventoryItemRow* table, InventoryItemRange* scan, s32 item)
{
    s32 index;
    s32 found;
    s32 i;

    found = 0;
    if (item >= 0xA0) {
        index = scan->firstRow;
        return Gp_FindScanQty(table, scan, &index, item);
    }
    for (i = scan->firstRow; i < scan->firstRow + scan->rowCount; i++) {
        if (table[i].itemId == item) {
            found = 1;
            break;
        }
    }
    return found;
}

EquipmentWeaponLoadOptionsTable Gp_RelatedQty1                             = { { { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 50, { 181, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 40, { 187, 0, 0 } }, { 0, { 0, 0, 0 } }, { 1, { 169, 170, 171 } }, { 30, { 189, 0, 0 } }, { 60, { 190, 0, 0 } }, { 50, { 181, 0, 0 } }, { 50, { 181, 0, 0 } }, { 50, { 181, 0, 0 } } } };
EquipmentWeaponSupply           Gp_ItemMaps[EQUIPMENT_WEAPON_SUPPLY_COUNT] = {
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x84, 0xB5, 0 }, // P229, Battery
    { EQUIPMENT_WEAPON_SUPPLY_PRIMARY, 0x95, 0xB9, 0 },   // Hypervelocity, Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x98, 0xBB, 0 }, // M4A1 Hammer, Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9B, 0xBD, 0 }, // M4A1 Pyke, Fuel
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9C, 0xBE, 0 }, // M4A1 Javelin, Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9D, 0xB5, 0 }, // MP5A5, Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9E, 0xB5, 0 }, // MP5A5(+1), Battery
    { EQUIPMENT_WEAPON_SUPPLY_SECONDARY, 0x9F, 0xB5, 0 }, // MP5A5(+2), Battery
};

void Gp_ApplyItemMap(void)
{
    s32                    i;
    EquipmentWeaponSupply* supply;
    EquipmentWeaponLoad*   slot;
    s32                    weaponItemId;

    // Install each built-in supply at its load's capacity.
    for (i = 0; i < EQUIPMENT_WEAPON_SUPPLY_COUNT; i++) {
        supply       = &Gp_ItemMaps[i];
        weaponItemId = supply->weaponItemId;
        slot         = gpItemSlot(weaponItemId);
        if (supply->supplyLoad == EQUIPMENT_WEAPON_SUPPLY_PRIMARY) {
            slot->primaryItemId = supply->supplyItemId;
            slot->primaryQty    = _gpRelatedQty(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_PRIMARY);
        } else {
            slot->secondaryItemId = supply->supplyItemId;
            slot->secondaryQty    = _gpRelatedQty(weaponItemId, EQUIPMENT_WEAPON_SUPPLY_SECONDARY);
        }
    }
}

s32 Gp_ConsumeSlotQty(s32 arg0, s32 arg1)
{
    EquipmentWeaponLoad* slot;
    s32*                 counter;
    McSaveData*          save;
    s32                  count;

    slot    = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[arg0 - EQUIPMENT_WEAPON_ITEM_FIRST];
    counter = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[arg0 - 0x80];

    if (arg1 == 1 && slot->primaryItemId != INVENTORY_ITEM_NONE && slot->primaryQty != 0) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) {
            slot->primaryQty--;
            Gp_ConsumeScanQty(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems, slot->primaryItemId, 1);
            count = *counter;
            if (count <= 0xF423E) {
                *counter = count + 1;
            }
        }
    } else if (arg1 == 0x101) {
        count = slot->secondaryItemId;
        if (count != 0) {
            if (count != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
                count = slot->secondaryQty;
                if (count != 0) {
                    save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
                    if (save->state.cheatMode == 0) {
                        slot->secondaryQty = count - 1;
                        Gp_ConsumeScanQty(&save->state.carriedItems, slot->secondaryItemId, 1);
                        count = *counter;
                        if (count <= 0xF423E) {
                            *counter = count + 1;
                        }
                    }
                }
            }
        }
    }

    if (!(arg1 & 0x100)) {
        return slot->primaryQty;
    }
    return slot->secondaryQty;
}

s32 Gp_EquipRelatedBank(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32                               index;
    InventoryItemRow*                 table;
    InventoryItemRange*               scan;
    EquipmentWeaponLoad*              slot;
    const EquipmentWeaponLoadOptions* row;
    s32                               maxQty;
    s32                               have;
    s32                               i;

    scan  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.carriedItems;
    table = Gp_GetItemTable(scan);
    if ((u32)(arg1 - EQUIPMENT_WEAPON_ITEM_FIRST) >= ARRAY_SIZE(Gp_RelatedQty0.rows)) {
        return -1;
    }
    if (_gpScanHeldQty(table, scan, arg1) <= 0) {
        return -1;
    }
    if (arg0 == 0) {
        row    = &Gp_RelatedQty0.rows[arg1 - EQUIPMENT_WEAPON_ITEM_FIRST];
        maxQty = _gpRelatedQty(arg1, 0);
    } else {
        row    = &Gp_RelatedQty1.rows[arg1 - EQUIPMENT_WEAPON_ITEM_FIRST];
        maxQty = _gpRelatedQty(arg1, 1);
    }
    for (i = 0; i < ARRAY_SIZE(row->acceptedItemIds); i++) {
        if (row->acceptedItemIds[i] == arg2) {
            break;
        }
    }
    if (i == ARRAY_SIZE(row->acceptedItemIds)) {
        return -1;
    }
    if (arg3 < 0) {
        arg3 = maxQty;
    }
    if (maxQty < arg3) {
        arg3 = maxQty;
    }
    index = scan->firstRow;
    slot  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[arg1 - EQUIPMENT_WEAPON_ITEM_FIRST];
    have  = (s16)Gp_FindScanQty(table, scan, &index, arg2);
    have -= Gp_CountEquippedRelated(scan, arg2);
    if (arg0 == 0) {
        if (slot->primaryItemId == arg2) {
            have += slot->primaryQty;
        }
    } else if (slot->secondaryItemId == arg2) {
        have += slot->secondaryQty;
    }
    if (have <= 0) {
        return -1;
    }
    if (arg3 != 0) {
        if (have < arg3) {
            arg3 = have;
        }
        if (arg0 == 0) {
            slot->primaryItemId = arg2;
            slot->primaryQty    = arg3;
        } else if (slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
            slot->secondaryItemId = arg2;
            slot->secondaryQty    = arg3;
        } else {
            arg3 = -1;
        }
        return arg3;
    }
    return 0;
}

s32 Gp_EquipRelatedItem(InventoryItemRange* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32                               index;
    InventoryItemRow*                 table;
    EquipmentWeaponLoad*              slot;
    const EquipmentWeaponLoadOptions* row;
    s32                               maxQty;
    s32                               have;
    s32                               useSecond;
    s32                               i;

    table     = Gp_GetItemTable(arg0);
    useSecond = 0;
    if ((u32)(arg2 - 0xA0) >= 0x20 || (u32)(arg1 - EQUIPMENT_WEAPON_ITEM_FIRST) >= ARRAY_SIZE(Gp_RelatedQty0.rows)) {
        return -1;
    }
    if (_gpScanHeldQty(table, arg0, arg1) <= 0) {
        return -1;
    }
    row    = &Gp_RelatedQty0.rows[arg1 - EQUIPMENT_WEAPON_ITEM_FIRST];
    maxQty = _gpRelatedQty(arg1, 0);
    for (i = 0; i < ARRAY_SIZE(row->acceptedItemIds); i++) {
        if (row->acceptedItemIds[i] == arg2) {
            break;
        }
    }
    if (i == ARRAY_SIZE(row->acceptedItemIds)) {
        useSecond = 1;
        row       = &Gp_RelatedQty1.rows[arg1 - EQUIPMENT_WEAPON_ITEM_FIRST];
        maxQty    = _gpRelatedQty(arg1, 1);
        for (i = 0; i < ARRAY_SIZE(row->acceptedItemIds); i++) {
            if (row->acceptedItemIds[i] == arg2) {
                break;
            }
        }
        if (i == ARRAY_SIZE(row->acceptedItemIds)) {
            return -1;
        }
    }
    if (arg3 < 0) {
        arg3 = maxQty;
    }
    if (maxQty < arg3) {
        arg3 = maxQty;
    }
    index = arg0->firstRow;
    slot  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponItems[arg1 - EQUIPMENT_WEAPON_ITEM_FIRST];
    have  = (s16)Gp_FindScanQty(table, arg0, &index, arg2);
    have -= Gp_CountEquippedRelated(arg0, arg2);
    if (slot->primaryItemId == arg2) {
        have += slot->primaryQty;
    }
    if (slot->secondaryItemId == arg2) {
        have += slot->secondaryQty;
    }
    if (have <= 0) {
        return -1;
    }
    if (arg3 != 0) {
        if (have < arg3) {
            arg3 = have;
        }
        if (useSecond == 0) {
            slot->primaryItemId = arg2;
            slot->primaryQty    = arg3;
        } else if (slot->secondaryItemId != EQUIPMENT_WEAPON_SECONDARY_UNAVAILABLE) {
            slot->secondaryItemId = arg2;
            slot->secondaryQty    = arg3;
        }
    } else {
        asm volatile("" : "=r"(arg3));
        arg3 = 0;
    }
    if (arg3 > 0) {
        Gp_SetItemSeenBit(arg2, 1);
    }
    return arg3;
}

u8 D_8010D318[8] = {
    143,
    147,
    148,
    152,
    153,
    154,
    155,
    156,
};

u8 D_8010D320[2] = { 0x80, 0x83 };

u8 D_8010D324[3] = { 0x9D, 0x9E, 0x9F };

PlayerModeBaseStats Gp_StatRows[4] = {
    { { 100 }, 30 },
    { { 100 }, 30 },
    { { 100 }, 10 },
    { { 50 }, 30 },
};

/* Count item `id` in saved rows 0..254 through a cleared range. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = INVENTORY_ITEM_RANGE_MAX_ROWS, Gp_SumScanQty(&(scan), (id)))

s32 func_800B7420(s32 arg0)
{
    InventoryItemRange scan;
    s32                i;
    s32                id;

    switch (arg0) {
        case 0x8F:
        case 0x93:
        case 0x94:
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
            for (i = 0; i < 8; i++) {
                id = D_8010D318[i];
                if (GP_TOTAL_QTY(scan, id)) {
                    return 1;
                }
            }
            return 0;

        case 0x80:
        case 0x83:
            for (i = 0; i < 2; i++) {
                id = D_8010D320[i];
                if (GP_TOTAL_QTY(scan, id)) {
                    return 1;
                }
            }
            return 0;

        case 0x9D:
        case 0x9E:
        case 0x9F:
            for (i = 0; i < 3; i++) {
                id = D_8010D324[i];
                if (GP_TOTAL_QTY(scan, id)) {
                    return 1;
                }
            }
            return 0;

        case 0x9:
            if (GP_TOTAL_QTY(scan, 0x9F)) {
                return 1;
            }
            if (GP_TOTAL_QTY(scan, 0x9E)) {
                if (GP_TOTAL_QTY(scan, 0x9)) {
                    return 1;
                }
            }
            return GP_TOTAL_QTY(scan, 0x9) >= 2;

        case 0xA:
            if (GP_TOTAL_QTY(scan, 0x94)) {
                return 1;
            }
            if (GP_TOTAL_QTY(scan, 0x93)) {
                if (GP_TOTAL_QTY(scan, 0xA)) {
                    return 1;
                }
            }
            return GP_TOTAL_QTY(scan, 0xA) >= 2;

        case 0xC:
            if (GP_TOTAL_QTY(scan, 0x80) || GP_TOTAL_QTY(scan, 0xC)) {
                return 1;
            }
            return 0;

        case 0x42:
            if (GP_TOTAL_QTY(scan, 0x98) || GP_TOTAL_QTY(scan, 0x42)) {
                return 1;
            }
            return 0;

        case 0x43:
            if (GP_TOTAL_QTY(scan, 0x9B) || GP_TOTAL_QTY(scan, 0x43)) {
                return 1;
            }
            return 0;

        case 0x44:
            if (GP_TOTAL_QTY(scan, 0x9C) || GP_TOTAL_QTY(scan, 0x44)) {
                return 1;
            }
            return 0;

        case 0x45:
            if (GP_TOTAL_QTY(scan, 0x9A) || GP_TOTAL_QTY(scan, 0x45)) {
                return 1;
            }
            return 0;

        case 0x46:
            if (GP_TOTAL_QTY(scan, 0x99) || GP_TOTAL_QTY(scan, 0x46)) {
                return 1;
            }
            return 0;

        default:
            if ((u32)(arg0 - 0x60) < 0x40) {
                return GP_TOTAL_QTY(scan, arg0);
            }
            return 0;
    }
}

void Gp_RecalcMaxMp(void)
{
    PlayerStatus*        cfg;
    McSaveData*          save;
    PlayerModeBaseStats* rows;
    s8*                  levels;
    s32                  acc;
    s32                  i;
    s32                  j;

    cfg    = &gPlayerStatus;
    acc    = 0;
    levels = (s8*)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    for (i = 0; i < 0xC; i++) {
        if (*levels > 0) {
            for (j = 0; j < *levels; j++) {
                acc += Gp_IdParamHi.rows[i * 3 + j + 1].column.mpBonus;
            }
        }
        levels++;
    }
    if (cfg->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
        acc += Gp_ModStatAttrs[cfg->armor - 1].mpBonus;
    }
    rows       = Gp_StatRows;
    save       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    acc       += rows[save->state.gameMode].baseMp;
    acc       += save->state.mpBonus;
    cfg->mpMax = acc;
    if ((s16)acc >= PLAYER_STATUS_STAT_MAX + 1) {
        cfg->mpMax = PLAYER_STATUS_STAT_MAX;
    }
    if (cfg->mp > cfg->mpMax) {
        cfg->mp = cfg->mpMax;
    }
}

void Gp_EquipMod(s32 arg0)
{
    PlayerStatus*       cfg;
    InventoryItemRow*   rec;
    InventoryItemRow*   tmp;
    InventoryItemRange* scan;
    s32                 i;

    cfg = &gPlayerStatus;
    if ((u32)(arg0 - 0x60) < 0x20U) {
        if (cfg->armor != (arg0 - 0x5F)) {
            InventoryItemRow* found;

            found = Gp_FindItemById(arg0);
            if (found != NULL) {
                found->attachSlot = INVENTORY_ATTACHMENT_EQUIPPED_ARMOR;
                if (cfg->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
                    found = Gp_FindItemById(cfg->armor + 0x5F);
                    if (found != NULL) {
                        found->attachSlot = INVENTORY_ATTACHMENT_NONE;
                    }
                }
                cfg->armor = arg0 - 0x5F;

                {
                    PlayerStatus*        p;
                    McSaveData*          save;
                    PlayerModeBaseStats* table;
                    u16                  val;

                    p        = &gPlayerStatus;
                    table    = Gp_StatRows;
                    save     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
                    val      = table[save->state.gameMode].baseHp.hp;
                    p->hpMax = val;
                    val     += save->state.hpBonus;
                    p->hpMax = val;
                    if (p->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
                        val     += Gp_ModStatAttrs[p->armor - 1].hpBonus;
                        p->hpMax = val;
                    }
                    if (p->hpMax >= PLAYER_STATUS_STAT_MAX + 1) {
                        p->hpMax = PLAYER_STATUS_STAT_MAX;
                    }
                    if (p->hp > p->hpMax) {
                        p->hp = p->hpMax;
                    }

                    scan = &save->state.carriedItems;
                    Gp_RecalcMaxMp();
                    switch (scan->tableId) {
                        case INVENTORY_ITEM_TABLE_AREA_GRANTS:
                            tmp = Gp_ItemTable2;
                            break;
                        case INVENTORY_ITEM_TABLE_INDIRECT:
                            tmp = Gp_ItemTable1;
                            break;
                        default:
                            tmp = save->state.itemRows;
                            break;
                    }
                }
                i   = 0;
                rec = &tmp[scan->firstRow];
                if (scan->rowCount != 0) {
                    do {
                        Gp_RefreshItemRow(rec);
                        i++;
                        rec++;
                    } while (i < scan->rowCount);
                }

                {
                    McSaveData* p;
                    s32         word;
                    s32         bit;

                    word = arg0 / 32;
                    bit  = 1 << (arg0 % 32);
                    if ((u32)arg0 < 0x180U) {
                        p                            = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
                        p->state.itemSeenBits[word] |= bit;
                    }
                }
            }
        }
    } else if (arg0 == 0) {
        McSaveData*          save;
        PlayerModeBaseStats* table;
        u16                  val;

        table      = Gp_StatRows;
        save       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        val        = table[save->state.gameMode].baseHp.hp;
        cfg->hpMax = val;
        val       += save->state.hpBonus;
        cfg->hpMax = val;
        if (cfg->armor != PLAYER_STATUS_EQUIPMENT_NONE) {
            val       += Gp_ModStatAttrs[cfg->armor - 1].hpBonus;
            cfg->hpMax = val;
        }
        if (cfg->hpMax >= PLAYER_STATUS_STAT_MAX + 1) {
            cfg->hpMax = PLAYER_STATUS_STAT_MAX;
        }
        if (cfg->hp > cfg->hpMax) {
            cfg->hp = cfg->hpMax;
        }
        Gp_RecalcMaxMp();
    } else {
        return;
    }
    Gp_HpMpWork.hp = cfg->hp;
    Gp_HpMpWork.mp = cfg->mp;
}

const char Gp_StrNotice2[8] = "Notice\0F";
