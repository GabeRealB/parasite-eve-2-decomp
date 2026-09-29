#include "gameplay/items.h"

#include <psyq/memory.h>

#include "types.h"

#include "gameplay/attachment_state.h"
#include "attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/inventory.h"
#include "inventory.h"
#include "item_use.h"
#include "items.h"
#include "gameplay/weapon_data.h"

#include "main/mc.h"
#include "main/wipsys.h"

McItemRec* Gp_ItemTable1;

extern u8 D_8010D318[8];

extern u8 D_8010D320[2];

extern u8 D_8010D324[3];

static inline s32 _gpRelatedQty(s32 item, s32 bank);

static inline s16 _gpScanHeldQty(McItemRec* table, McItemScan* scan, s32 item);

static inline s32 _gpRelatedQty(s32 item, s32 bank)
{
    s32 ret;

    item -= 0x80;
    ret   = 0;
    if ((u32)item < 0x20) {
        if (bank == 0) {
            ret = Gp_RelatedQty0.rows[item].field_0;
        } else {
            ret = Gp_RelatedQty1.rows[item].field_0;
        }
    }
    return ret;
}
static inline s16 _gpScanHeldQty(McItemRec* table, McItemScan* scan, s32 item)
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

GpRelatedItemTable Gp_RelatedQty1 = { { { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 50, { 181, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 0, { 0, 0, 0 } }, { 40, { 187, 0, 0 } }, { 0, { 0, 0, 0 } }, { 1, { 169, 170, 171 } }, { 30, { 189, 0, 0 } }, { 60, { 190, 0, 0 } }, { 50, { 181, 0, 0 } }, { 50, { 181, 0, 0 } }, { 50, { 181, 0, 0 } } } };
GpItemMap          Gp_ItemMaps[8] = {
    { 1, 132, 181, 0 },
    { 0, 149, 185, 0 },
    { 1, 152, 187, 0 },
    { 1, 155, 189, 0 },
    { 1, 156, 190, 0 },
    { 1, 157, 181, 0 },
    { 1, 158, 181, 0 },
    { 1, 159, 181, 0 },
};

void Gp_ApplyItemMap(void)
{
    s32         i;
    GpItemMap*  map;
    McItemSlot* slot;
    s32         id;

    for (i = 0; i < 8; i++) {
        map  = &Gp_ItemMaps[i];
        id   = map->field_1;
        slot = gpItemSlot(id);
        if (map->field_0 == 0) {
            slot->ammoId  = map->field_2;
            slot->ammoQty = _gpRelatedQty(id, 0);
        } else {
            slot->attachId  = map->field_2;
            slot->attachQty = _gpRelatedQty(id, 1);
        }
    }
}

s32 Gp_ConsumeSlotQty(s32 arg0, s32 arg1)
{
    McItemSlot* slot;
    s32*        counter;
    McSaveData* save;
    s32         count;

    slot    = &Mc_SaveData[0].state.weaponItems[arg0 - 0x80];
    counter = &Mc_SaveData[0].state.weaponUseCounts[arg0 - 0x80];

    if (arg1 == 1) {
        if (slot->ammoId != 0) {
            count = slot->ammoQty;
            if (count != 0) {
                if (Mc_SaveData[0].state.cheatMode == 0) {
                    slot->ammoQty = count - 1;
                    Gp_ConsumeScanQty(&Mc_SaveData[0].state.carriedItems, slot->ammoId, 1);
                    count = *counter;
                    if (count <= 0xF423E) {
                        *counter = count + 1;
                    }
                }
                goto done;
            }
        }
    }
    if (arg1 == 0x101) {
        count = slot->attachId;
        if (count != 0) {
            if (count != 0xFF) {
                count = slot->attachQty;
                if (count != 0) {
                    save = &Mc_SaveData[0];
                    if (save->state.cheatMode == 0) {
                        slot->attachQty = count - 1;
                        Gp_ConsumeScanQty(&save->state.carriedItems, slot->attachId, 1);
                        count = *counter;
                        if (count <= 0xF423E) {
                            *counter = count + 1;
                        }
                    }
                }
            }
        }
    }

done:
    if (!(arg1 & 0x100)) {
        return slot->ammoQty;
    }
    return slot->attachQty;
}

s32 Gp_EquipRelatedBank(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32         index;
    McItemRec*  table;
    McItemScan* scan;
    McItemSlot* slot;
    GpItemQty*  row;
    s32         maxQty;
    s32         have;
    s32         i;

    scan  = &Mc_SaveData[0].state.carriedItems;
    table = Gp_GetItemTable(scan);
    if ((u32)(arg1 - 0x80) >= 0x20) {
        return -1;
    }
    if (_gpScanHeldQty(table, scan, arg1) <= 0) {
        return -1;
    }
    if (arg0 == 0) {
        row    = &Gp_RelatedQty0.rows[arg1 - 0x80];
        maxQty = _gpRelatedQty(arg1, 0);
    } else {
        row    = &Gp_RelatedQty1.rows[arg1 - 0x80];
        maxQty = _gpRelatedQty(arg1, 1);
    }
    for (i = 0; i < 3; i++) {
        if (row->related[i] == arg2) {
            break;
        }
    }
    if (i == 3) {
        return -1;
    }
    if (arg3 < 0) {
        arg3 = maxQty;
    }
    if (maxQty < arg3) {
        arg3 = maxQty;
    }
    index = scan->firstRow;
    slot  = &Mc_SaveData[0].state.weaponItems[arg1 - 0x80];
    have  = (s16)Gp_FindScanQty(table, scan, &index, arg2);
    have -= Gp_CountEquippedRelated(scan, arg2);
    if (arg0 == 0) {
        if (slot->ammoId == arg2) {
            have += slot->ammoQty;
        }
    } else if (slot->attachId == arg2) {
        have += slot->attachQty;
    }
    if (have <= 0) {
        return -1;
    }
    if (arg3 != 0) {
        if (have < arg3) {
            arg3 = have;
        }
        if (arg0 == 0) {
            slot->ammoId  = arg2;
            slot->ammoQty = arg3;
        } else if (slot->attachId != 0xFF) {
            slot->attachId  = arg2;
            slot->attachQty = arg3;
        } else {
            arg3 = -1;
        }
        return arg3;
    }
    return 0;
}

s32 Gp_EquipRelatedItem(McItemScan* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32         index;
    McItemRec*  table;
    McItemSlot* slot;
    GpItemQty*  row;
    s32         maxQty;
    s32         have;
    s32         useSecond;
    s32         i;

    table     = Gp_GetItemTable(arg0);
    useSecond = 0;
    if ((u32)(arg2 - 0xA0) >= 0x20 || (u32)(arg1 - 0x80) >= 0x20) {
        return -1;
    }
    if (_gpScanHeldQty(table, arg0, arg1) <= 0) {
        return -1;
    }
    row    = &Gp_RelatedQty0.rows[arg1 - 0x80];
    maxQty = _gpRelatedQty(arg1, 0);
    for (i = 0; i < 3; i++) {
        if (row->related[i] == arg2) {
            break;
        }
    }
    if (i == 3) {
        useSecond = 1;
        row       = &Gp_RelatedQty1.rows[arg1 - 0x80];
        maxQty    = _gpRelatedQty(arg1, 1);
        for (i = 0; i < 3; i++) {
            if (row->related[i] == arg2) {
                break;
            }
        }
        if (i == 3) {
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
    slot  = &Mc_SaveData[0].state.weaponItems[arg1 - 0x80];
    have  = (s16)Gp_FindScanQty(table, arg0, &index, arg2);
    have -= Gp_CountEquippedRelated(arg0, arg2);
    if (slot->ammoId == arg2) {
        have += slot->ammoQty;
    }
    if (slot->attachId == arg2) {
        have += slot->attachQty;
    }
    if (have <= 0) {
        return -1;
    }
    if (arg3 != 0) {
        if (have < arg3) {
            arg3 = have;
        }
        if (useSecond == 0) {
            slot->ammoId  = arg2;
            slot->ammoQty = arg3;
        } else if (slot->attachId != 0xFF) {
            slot->attachId  = arg2;
            slot->attachQty = arg3;
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

GpStatRow Gp_StatRows[4] = {
    { { 100 }, 30 },
    { { 100 }, 30 },
    { { 100 }, 10 },
    { { 50 }, 30 },
};

/* Total quantity of item `id` held, via a fresh scan covering every row. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = 0xFF, Gp_SumScanQty(&(scan), (id)))

s32 func_800B7420(s32 arg0)
{
    McItemScan scan;
    s32        i;
    s32        id;

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
    PlayerStatus* cfg;
    McSaveData*   save;
    GpStatRow*    rows;
    s8*           levels;
    s32           acc;
    s32           i;
    s32           j;

    cfg    = &Player_Status;
    acc    = 0;
    levels = (s8*)Mc_SaveData[0].state.attachLevels;
    for (i = 0; i < 0xC; i++) {
        if (*levels > 0) {
            for (j = 0; j < *levels; j++) {
                acc += Gp_IdParamHi.rows[i * 3 + j + 1].field[1];
            }
        }
        levels++;
    }
    if (cfg->armor != 0) {
        acc += Gp_ModStatAttrs[cfg->armor - 1].field_6;
    }
    rows       = Gp_StatRows;
    save       = &Mc_SaveData[0];
    acc       += rows[save->state.gameMode].field_4;
    acc       += save->state.mpBonus;
    cfg->mpMax = acc;
    if ((s16)acc >= 0xFB) {
        cfg->mpMax = 0xFA;
    }
    if (cfg->mp > cfg->mpMax) {
        cfg->mp = cfg->mpMax;
    }
}

void Gp_EquipMod(s32 arg0)
{
    PlayerStatus* cfg;
    McItemRec*    rec;
    McItemRec*    tmp;
    McItemScan*   scan;
    s32           i;

    cfg = &Player_Status;
    if ((u32)(arg0 - 0x60) < 0x20U) {
        if (cfg->armor != (arg0 - 0x5F)) {
            McItemRec* found;

            found = Gp_FindItemById(arg0);
            if (found != NULL) {
                found->attachSlot = -1;
                if (cfg->armor != 0) {
                    found = Gp_FindItemById(cfg->armor + 0x5F);
                    if (found != NULL) {
                        found->attachSlot = 0;
                    }
                }
                cfg->armor = arg0 - 0x5F;

                {
                    PlayerStatus* p;
                    McSaveData*   save;
                    GpStatRow*    table;
                    u16           val;

                    p        = &Player_Status;
                    table    = Gp_StatRows;
                    save     = &Mc_SaveData[0];
                    val      = table[save->state.gameMode].base.half;
                    p->hpMax = val;
                    val     += save->state.hpBonus;
                    p->hpMax = val;
                    if (p->armor != 0) {
                        val     += Gp_ModStatAttrs[p->armor - 1].field_4;
                        p->hpMax = val;
                    }
                    if (p->hpMax >= 0xFB) {
                        p->hpMax = 0xFA;
                    }
                    if (p->hp > p->hpMax) {
                        p->hp = p->hpMax;
                    }

                    scan = &save->state.carriedItems;
                    Gp_RecalcMaxMp();
                    switch (scan->table) {
                        case 2:
                            tmp = Gp_ItemTable2;
                            break;
                        case 1:
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
                        p                            = &Mc_SaveData[0];
                        p->state.itemSeenBits[word] |= bit;
                    }
                }
            }
        }
    } else if (arg0 == 0) {
        McSaveData* save;
        GpStatRow*  table;
        u16         val;

        table      = Gp_StatRows;
        save       = &Mc_SaveData[0];
        val        = table[save->state.gameMode].base.half;
        cfg->hpMax = val;
        val       += save->state.hpBonus;
        cfg->hpMax = val;
        if (cfg->armor != 0) {
            val       += Gp_ModStatAttrs[cfg->armor - 1].field_4;
            cfg->hpMax = val;
        }
        if (cfg->hpMax >= 0xFB) {
            cfg->hpMax = 0xFA;
        }
        if (cfg->hp > cfg->hpMax) {
            cfg->hp = cfg->hpMax;
        }
        Gp_RecalcMaxMp();
    } else {
        return;
    }
    Gp_HpMpWork.field_0 = cfg->hp;
    Gp_HpMpWork.field_4 = cfg->mp;
}

const char Gp_StrNotice2[8] = "Notice\0F";
