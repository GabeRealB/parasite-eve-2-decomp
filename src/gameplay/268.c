#include "common.h"

#include <psyq/libgte.h>
#include <psyq/memory.h>

#include "debug/debug.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/4CC.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern u16          Gp_PlayTimeMark;
extern McItemRec*   Gp_SelItemRec;
extern UiObjectDesc Gp_BoostPanelDesc;
extern u8           Gp_DebugAttachLevels[];
static const char   Gp_StrNotice2[];
extern u8           Gp_StrMore[];
extern u8           Gp_StrAttachAvail[];
extern u8           D_8010D318[];
extern u8           D_8010D320[];
extern u8           D_8010D324[];

void func_80180804(void);
void func_8017EA68(void);
void func_80181468(void);
void func_8017EA90(void);
void func_8017E9E8(void);
void func_80181364(void);
void func_8017EA58(void);
void func_8017E9F8(void);
void func_8017EAE0(void);
void func_8018138C(void);
void func_8017EA74(void);
void func_8017EA78(void);
void func_8017EB2C(void);
void func_8017EDE8(void);
void func_8017EAB4(void);
void func_8017EA64(void);
void func_8017EC04(void);
void func_8017EAC4(void);
void func_8017EA60(void);
void Gp_EquipHeld(s32 arg0);
void Gp_ApplyItemMap(void);
void Gp_SetCollectedBit(s32 arg0);
s32  Gp_FindScanQty(McItemRec* arg0, McItemScan* arg1, s32* arg2, s32 arg3);
void Gp_DrawHpMpStats(UiPanel* arg0, s32 arg1);
void func_801061F0(void);
void Gp_NoticePanelTask(Task* arg0);

/* Total quantity of item `id` held, via a fresh scan covering every row. */
#define GP_TOTAL_QTY(scan, id) (memset(&(scan), 0, sizeof(scan)), (scan).rowCount = 0xFF, Gp_SumScanQty(&(scan), (id)))

static void Gp_ApplyBit2List(GpBit2List* table, u32* dest);
static void Gp_ClearCollectedBits(void);
static void Gp_SetPlayerScan(s32 arg0);

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
    levels = (s8*)Mc_SaveData[0].attachLevels;
    for (i = 0; i < 0xC; i++) {
        if (*levels > 0) {
            for (j = 0; j < *levels; j++) {
                acc += Gp_IdParamHi[i * 3 + j + 1].field[1];
            }
        }
        levels++;
    }
    if (cfg->armor != 0) {
        acc += Gp_ModStatAttrs[cfg->armor - 1].field_6;
    }
    rows       = Gp_StatRows;
    save       = &Mc_SaveData[0];
    acc       += rows[save->gameMode].field_4;
    acc       += save->mpBonus;
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
                    val      = table[save->gameMode].base.half;
                    p->hpMax = val;
                    val     += save->hpBonus;
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

                    scan = &save->carriedItems;
                    Gp_RecalcMaxMp();
                    switch (scan->table) {
                        case 2:
                            tmp = Gp_ItemTable2;
                            break;
                        case 1:
                            tmp = Gp_ItemTable1;
                            break;
                        default:
                            tmp = save->itemRows;
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
                        p                      = &Mc_SaveData[0];
                        p->itemSeenBits[word] |= bit;
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
        val        = table[save->gameMode].base.half;
        cfg->hpMax = val;
        val       += save->hpBonus;
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

void Gp_InitStarterInv(void)
{
    McItemScan*   scan;
    McSaveData*   save;
    PlayerStatus* cfg;
    PlayerStatus* cfg2;
    McItemRec*    tmp;
    McItemRec*    rec;
    McItemRec*    added;
    McItemScan**  scans;
    McItemScan*   dest;
    McItemSlot*   slots;
    s32           i;
    s32           j;
    u8            item;
    s32           three;
    u16           hp;
    u16           mp;
    s32           flag105;
    s32           flag107;

    scan                    = &Mc_SaveData[0].carriedItems;
    save                    = &Mc_SaveData[0];
    save->itemLevelBonus[5] = 0;
    save->itemLevelBonus[0] = 0;
    cfg                     = &Player_Status;
    switch (scan->table) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData[0].itemRows;
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
    slots = Mc_SaveData[0].weaponItems;
    for (j = 0; j < 0x20; j++) {
        slots->ammoId    = 0;
        slots->ammoQty   = 0;
        slots->attachId  = 0xFF;
        slots->attachQty = 0;
        if (j == 0x1A) {
            slots->attachId  = 0;
            slots->attachQty = 0;
        }
        slots->field_4 = 0;
        slots++;
    }
    three = 3;
    Gp_ApplyItemMap();
    Gp_GiveItem(scan, 0x63, 1);
    gGameSession->loadedWeaponFamily = -1;
    cfg->weapon                      = 0;
    cfg->field_26                    = three;
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
    cfg2     = &Player_Status;
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

/* Gives `scan` one `weapon` and loads it with `ammo`. */
#define GP_GIVE_LOADED(scan, weapon, ammo)           \
    do {                                             \
        Gp_GiveItem(scan, weapon, 1);                \
        Gp_EquipRelatedItem(scan, weapon, ammo, -1); \
    } while (0)

void func_800B8014(void)
{
    McItemRec*    rec;
    McSaveData*   save;
    GpItemDesc*   desc;
    u8*           str;
    McItemSlot*   slots;
    McItemScan*   scan;
    McItemScan**  scans;
    PlayerStatus* cfg;
    s32*          header;
    s32           word;
    s32           i;
    s32           j;
    s32           count;
    s32           row;
    s32           col;

    for (j = 0, rec = Mc_SaveData[0].itemRows; j < 0x100; j++) {
        rec->itemId = 0;
        rec->qty    = 0;
        rec++;
    }
    for (i = 0x5F; i >= 0; i--) {
        Mc_SaveData[0].itemSeenBits[i] = 0;
    }

    i = 0;
    do {
        count = 3;
        if (i < 0x100) {
            desc = &Gp_ItemDescs[i];
        } else {
            desc = &Gp_ItemDescsHi[i];
        }
        str = desc->field_4;
        while (count > 0) {
            if (*str == 0 || *str == 0xA) {
                count--;
            }
            str++;
        }
        if (*str == 0xA) {
            Gp_SetItemSeenBit(i, 1);
        }
        i++;
    } while (i < 0x180);
    Gp_ClearCollectedBits();
    slots = Mc_SaveData[0].weaponItems;
    for (j = 0; j < 0x20; j++) {
        slots->ammoId    = 0;
        slots->ammoQty   = 0;
        slots->attachId  = 0xFF;
        slots->attachQty = 0;
        if (j == 0x1A) {
            slots->attachId  = 0;
            slots->attachQty = 0;
        }
        slots->field_4 = 0;
        slots++;
    }
    Gp_ApplyItemMap();
    Mc_SaveData[0].carriedItems.firstRow = 0;
    Mc_SaveData[0].carriedItems.rowCount = 0x14;
    Mc_SaveData[0].carriedItems.table    = 0;
    for (row = 0; row < 4; row++) {
        for (col = 0; col < 3; col++) {
            Mc_SaveData[0].attachLevels[col + row * 3] = 0;
        }
    }
    save = &Mc_SaveData[0];
    SOFT_TOUCH_REG(save);
    scan                  = &save->carriedItems;
    save->attachLevels[0] = 1;
    cfg                   = &Player_Status;
    if (save->clearCount == 0) {
        cfg->bp = 0xC8;
    }
    Gp_ClearScanItems(scan);
    Gp_GiveItem(scan, 0x60, 1);
    Gp_EquipMod(0x60);
    cfg->hp = cfg->hpMax;
    cfg->mp = cfg->mpMax;
    Gp_GiveItem(scan, 0x92, 1);
    Gp_GiveItem(scan, 0x40, 1)->attachSlot    = 1;
    Gp_GiveItem(scan, 0xA0, 0x64)->attachSlot = 2;
    GP_GIVE_LOADED(scan, 0x81, 0xA0);
    Gp_GiveItem(scan, 2, 1)->attachSlot = 3;
    scans                               = Gp_ScanPtrs;
    scan                                = scans[1];
    Gp_ClearScanItems(scan);
    Gp_GiveItem(scan, 1, 1);
    Gp_GiveItem(scan, 1, 1);
    Gp_GiveItem(scan, 4, 1);
    scan = scans[2];
    Gp_ClearScanItems(scan);
    Gp_GiveItem(scan, 1, 1);
    Gp_GiveItem(scan, 1, 1);
    Gp_ClearScanItems(scans[3]);
    scan = scans[4];
    Gp_ClearScanItems(scan);
    Gp_GiveItem(scan, 0xA0, -1);
    Gp_GiveItem(scan, 4, 1);
    Gp_GiveItem(scan, 4, 1);
    Gp_ClearScanItems(scans[6]);
    Gp_ClearScanItems(scans[5]);
    scan = scans[8];
    Gp_ClearScanItems(scan);
    Gp_GiveItem(scan, 0xAC, 0x14);
    Gp_GiveItem(scan, 0xA9, 8);
    Gp_SetCollectedBit(0x106);
    header = (s32*)&Mc_SaveData[0].at4.loc.view;
    word   = *header;
    word  &= 0xFFFF0000;
    if (word == 0x01140000) {
        Gp_ResetInventory();
        Gp_GiveItem(&Mc_SaveData[0].carriedItems, 0x81, 1);
        Gp_EquipHeld(0x81);
    }
}

/* Item table a scan window lies in. */
static inline McItemRec* _gpScanTable(McItemScan* scan)
{
    McItemRec* table;

    switch (scan->table) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData[0].itemRows;
            break;
    }
    return table;
}

void Gp_MoveItemSlot(McItemScan* scan, s32 from, s32 to)
{
    McItemRec* table;
    McItemRec  saved;
    s32        i;

    table = _gpScanTable(scan);
    if (from == to) {
        return;
    }

    from += scan->firstRow;
    to   += scan->firstRow;

    if (from < to) {
        saved              = table[from];
        table[from].itemId = 0;
        table[from].qty    = 0;
        for (i = to; from < i; i--) {
            if (table[i].itemId == 0) {
                break;
            }
        }
        for (; i < to; i++) {
            table[i] = table[i + 1];
        }
    } else {
        saved              = table[from];
        table[from].itemId = 0;
        table[from].qty    = 0;
        for (i = to; i < from; i++) {
            if (table[i].itemId == 0) {
                break;
            }
        }
        for (; to < i; i--) {
            table[i] = table[i - 1];
        }
    }
    table[to] = saved;
}

void Gp_SortItems(McItemScan* arg0, s32 arg1)
{
    McItemRec*          tmp;
    register McItemRec* table;
    McItemRec*          rec;
    McItemRec*          other;
    McItemRec           saved;
    s32                 i;
    s32                 j;
    s32                 key;
    s32                 minKey;
    s32                 id;
    s32                 idx;
    s32                 next;
    s32                 count;
    s32                 dummy5;
    s32                 dummy6;
    s32                 dummy7;

    i = 0;
    if ((arg0->rowCount - 1) > 0) {
        do {
            switch (arg0->table) {
                case 2:
                    tmp = Gp_ItemTable2;
                    break;
                case 1:
                    tmp = Gp_ItemTable1;
                    break;
                default:
                    tmp = Mc_SaveData[0].itemRows;
                    break;
            }
            table = tmp;
            rec   = (McItemRec*)((s32)table + (arg0->firstRow << 2));
            rec   = (McItemRec*)((s32)rec + (i << 2));
            id    = rec->itemId;

            key = 0;
            if (id == 0) {
                key = 0x1000;
            } else if ((u32)(id - 1) < 0x5F) {
                key = Gp_ItemSortKey0[id];
            } else {
                idx = id - 0x60;
                if ((u32)idx < 0x20) {
                    key = Gp_ItemSortKey60[idx];
                } else {
                    idx = id - 0x80;
                    if ((u32)idx < 0x20) {
                        key = Gp_ItemSortKey80[idx];
                    } else {
                        idx = id - 0xA0;
                        if ((u32)idx < 0x20) {
                            key = Gp_ItemSortKeyA0[idx];
                        }
                    }
                }
            }
            if (key == 0) {
                key = id + 0x100;
            }
            minKey = key;

            if (arg0->table != 1) {
                tmp = Mc_SaveData[0].itemRows;
                if (arg0->table == 2) {
                    tmp = Gp_ItemTable2;
                }
            } else {
                tmp = Gp_ItemTable1;
            }
            table = tmp;
            j     = i + 1;
            other = (McItemRec*)((s32)table + (arg0->firstRow << 2));
            next  = (i << 2) + 4;
            other = (McItemRec*)((s32)other + next);
            if (j < arg0->rowCount) {
                do {
                    id = other->itemId;

                    key = 0;
                    if (id == 0) {
                        key = 0x1000;
                    } else if ((u32)(id - 1) < 0x5F) {
                        key = Gp_ItemSortKey0[id];
                    } else {
                        idx = id - 0x60;
                        if ((u32)idx < 0x20) {
                            key = Gp_ItemSortKey60[idx];
                        } else {
                            idx = id - 0x80;
                            if ((u32)idx < 0x20) {
                                key = Gp_ItemSortKey80[idx];
                            } else {
                                idx = id - 0xA0;
                                if ((u32)idx < 0x20) {
                                    key = Gp_ItemSortKeyA0[idx];
                                }
                            }
                        }
                    }
                    if (key == 0) {
                        key = id + 0x100;
                    }
                    if (key < minKey) {
                        minKey = key;
                        saved  = *rec;
                        *rec   = *other;
                        *other = saved;
                    }
                    count = arg0->rowCount;
                    j++;
                    other++;
                } while (j < count);
            }
            count = arg0->rowCount;
            i++;
        } while (i < (count - 1));
    }
}

/// True if `arg2` of item `arg1` can be added to the item table selected
/// by `arg0`. Ids `>= 0x100` always succeed. Ids `0xA0..0xFF` stack onto
/// an existing row when `qty + arg2` fits `Gp_StackLimits[id-0xA0].field_2`;
/// `arg2 < 0` uses that row's `field_0` as the addend. Other ids need a
/// free slot.
static s32 Gp_CanAddItemQty(McItemScan* arg0, s32 arg1, s32 arg2)
{
    McItemRec* tmp;
    McItemRec* table;
    McItemRec* rec;
    s32        i;
    s32        occupied;
    s32        count;
    s32        start;
    s32        limit;
    s32        used;
    s32        found;
    McItemRec* table2;
    McItemRec* walker;
    s32        count2;
    s32        start2;
    GpItemA0*  p;
    s32        idx;
    GpItemA0*  cap;
    s32        capacity;

    switch (arg0->table) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData[0].itemRows;
            break;
    }
    table    = tmp;
    i        = 0;
    count    = arg0->rowCount;
    start    = arg0->firstRow;
    occupied = i;
    if (count != 0) {
        limit = count;
        rec   = (McItemRec*)((start << 2) + (s32)table);
        do {
            if (rec->itemId != 0) {
                occupied++;
            }
            i++;
            rec++;
        } while (i < limit);
    }

    capacity = arg0->rowCount;
    used     = occupied;
    if (arg1 >= 0x100) {
        return 1;
    }

    if (arg1 >= 0xA0) {
        found  = 0;
        start2 = arg0->firstRow;
        switch (arg0->table) {
            case 2:
                table2 = Gp_ItemTable2;
                break;
            case 1:
                table2 = Gp_ItemTable1;
                break;
            default:
                table2 = Mc_SaveData[0].itemRows;
                break;
        }
        if (arg2 < 0) {
            arg2 = Gp_StackLimits[arg1 - 0xA0].field_0;
        }
        i      = 0;
        count2 = arg0->rowCount;
        if (count2 != 0) {
            p      = Gp_StackLimits;
            idx    = arg1 - 0xA0;
            cap    = (GpItemA0*)((idx << 2) + (s32)p);
            walker = (McItemRec*)((start2 << 2) + (s32)table2);
            do {
                if (walker->itemId == arg1) {
                    found = 2;
                    if (cap->field_2 >= walker->qty + arg2) {
                        found = 1;
                    }
                    break;
                }
                i++;
                walker++;
            } while (i < count2);
        }

        if (found == 0) {
            used++;
        }
        if (found == 2) {
            return 0;
        }
    } else {
        used++;
    }
    return used <= capacity;
}

s32 Gp_CanAddItem(McItemScan* arg0, s32 arg1)
{
    McItemRec* tmp;
    McItemRec* table;
    McItemRec* rec;
    s32        i;
    s32        occupied;
    s32        count;
    s32        start;
    s32        limit;
    s32        used;
    s32        found;
    McItemRec* table2;
    McItemRec* walker;
    s32        count2;
    s32        start2;
    GpItemA0*  p;
    s32        idx;
    GpItemA0*  cap;
    s32        capacity;

    switch (arg0->table) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData[0].itemRows;
            break;
    }
    table    = tmp;
    i        = 0;
    count    = arg0->rowCount;
    start    = arg0->firstRow;
    occupied = i;
    if (count != 0) {
        limit = count;
        rec   = (McItemRec*)((start << 2) + (s32)table);
        do {
            if (rec->itemId != 0) {
                occupied++;
            }
            i++;
            rec++;
        } while (i < limit);
    }

    capacity = arg0->rowCount;
    used     = occupied;
    if (arg1 >= 0x100) {
        return 1;
    }

    if (arg1 >= 0xA0) {
        found  = 0;
        start2 = arg0->firstRow;
        switch (arg0->table) {
            case 2:
                table2 = Gp_ItemTable2;
                break;
            case 1:
                table2 = Gp_ItemTable1;
                break;
            default:
                table2 = Mc_SaveData[0].itemRows;
                break;
        }
        i      = 0;
        count2 = arg0->rowCount;
        if (count2 != 0) {
            p      = Gp_StackLimits;
            idx    = arg1 - 0xA0;
            cap    = (GpItemA0*)((idx << 2) + (s32)p);
            walker = (McItemRec*)((start2 << 2) + (s32)table2);
            do {
                if (walker->itemId == arg1) {
                    if (walker->qty < cap->field_2) {
                        found = 1;
                    } else {
                        found = 2;
                    }
                    break;
                }
                i++;
                walker++;
            } while (i < count2);
        }

        if (found == 0) {
            used++;
        }
        if (found == 2) {
            return 0;
        }
    } else {
        used++;
    }
    return used <= capacity;
}

McItemRec* Gp_SetScanItem(McItemScan* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    McItemRec* table;
    McItemRec* dest;
    s32        row;
    s32        i;
    s32        item;
    s32        qty;

    table = _gpScanTable(arg0);
    if ((u32)(arg2 - 0xA0) < 0x20U) {
        dest = Gp_GiveItem(arg0, arg2, arg3);
        i    = arg0->firstRow;
        if (table[i + arg1].itemId == 0) {
            row = i;
            for (i = 0; i < arg0->rowCount; i++, row++) {
                if (table[row].itemId == arg2) {
                    if (i == arg1) {
                        break;
                    }
                    dest                             = &table[arg0->firstRow + arg1];
                    dest->itemId                     = arg2;
                    table[arg0->firstRow + arg1].qty = table[row].qty;
                    table[row].itemId                = 0;
                    table[row].qty                   = 0;
                    break;
                }
            }
        }
    } else {
        row = arg0->firstRow + arg1;
        if (table[row].itemId == 0) {
            dest         = &table[row];
            dest->itemId = arg2;
            dest->qty    = 1;
        } else {
            dest         = &table[row];
            item         = dest->itemId;
            qty          = dest->qty;
            dest->itemId = arg2;
            dest->qty    = 1;
            Gp_GiveItem(arg0, item, qty);
        }
    }
    return dest;
}

/// Adds `arg2` of item `arg1` to the item table selected by `arg0`.
/// Ids `0xA0..0xBF` stack onto an existing row, clamped to
/// `Gp_StackLimits[id-0xA0].field_2`. `arg2 < 0` uses that row's `field_0`
/// as the count, or `field_2` when `arg2 == -2`; out-of-range ids use 1.
/// Other ids take the first free slot with quantity 1. Returns the
/// written row, or NULL if none was free.
static McItemRec* Gp_AddItem(McItemScan* arg0, s32 arg1, s32 arg2)
{
    McItemRec* table;
    McItemRec* dest;
    s32        found;
    s32        row;
    s32        i;

    table = _gpScanTable(arg0);
    dest  = NULL;
    if (arg2 < 0) {
        if ((u32)(arg1 - 0xA0) < 0x20) {
            if (arg2 == -2) {
                arg2 = Gp_StackLimits[arg1 - 0xA0].field_2;
            } else {
                arg2 = Gp_StackLimits[arg1 - 0xA0].field_0;
            }
        } else {
            arg2 = 1;
        }
    }

    row   = arg0->firstRow;
    found = 0;
    if ((u32)(arg1 - 0xA0) < 0x20) {
        for (i = 0; i < arg0->rowCount; i++, row++) {
            if (table[row].itemId == arg1) {
                arg2 += table[row].qty;
                if (Gp_StackLimits[arg1 - 0xA0].field_2 < arg2) {
                    arg2 = Gp_StackLimits[arg1 - 0xA0].field_2;
                }
                table[row].qty = arg2;
                dest           = &table[row];
                found          = 1;
                break;
            }
        }
        if (found) {
            return dest;
        }
        row = arg0->firstRow;
        for (i = 0; i < arg0->rowCount; i++, row++) {
            if (table[row].itemId == 0) {
                table[row].itemId = arg1;
                if (Gp_StackLimits[arg1 - 0xA0].field_2 < arg2) {
                    arg2 = Gp_StackLimits[arg1 - 0xA0].field_2;
                }
                dest             = &table[row];
                dest->qty        = arg2;
                dest->attachSlot = 0;
                break;
            }
        }
    } else {
        for (i = 0; i < arg0->rowCount; i++, row++) {
            if (table[row].itemId == 0) {
                dest             = &table[row];
                dest->itemId     = arg1;
                dest->qty        = 1;
                dest->attachSlot = 0;
                break;
            }
        }
    }
    return dest;
}

/// Whether item `item` has been seen. Ids at or above 0x180 have no bit in
/// the save and always count as seen. Inline form of `Gp_HasItemSeenBit`.
static inline s32 _gpHasItemSeenBit(s32 item)
{
    McSaveData* p;
    s32         word;
    s32         bit;
    s32         val;

    word = item / 32;
    bit  = 1 << (item % 32);
    if ((u32)item >= 0x180) {
        return 1;
    }
    p   = &Mc_SaveData[0];
    val = p->itemSeenBits[word] & bit;
    return val != 0;
}

char* Gp_GetItemText(s32 arg0, s32 arg1, s32 arg2)
{
    s8*         str;
    GpItemDesc* desc;
    s32         c;
    s32         n;
    s32         row;
    s32         col;
    s32         id;
    s32         ofs;

    if (arg0 >= 0x500) {
        str = Gp_ItemTextHi[arg0 - 0x500];
    } else if (arg0 >= 0x300) {
        // A packed id: bits 4-7 and 2-3 pick a run of three entries starting
        // at id 0xF, bits 0-1 the entry within it (1-3, with 0 read as 1).
        n   = arg0 & 3;
        row = (arg0 & 0xF0) >> 4;
        col = (arg0 & 0xC) >> 2;
        if (n == 0) {
            n = 1;
        }
        id  = (row * 3 + col) * 3;
        ofs = n + 0xE;
        str = Gp_GetItemText(id + ofs, arg1, 1);
    } else {
        if (arg0 < 0x100) {
            desc = &Gp_ItemDescs[arg0];
        } else {
            desc = &Gp_ItemDescsHi[arg0];
        }
        if (arg2 == 0) {
            arg2 = _gpHasItemSeenBit(arg0);
        }
        str = desc->field_4;
        if (arg1 >= 3) {
            arg1 = 0;
        }
        if (arg2 == 0) {
            arg1 += 3;
        }
        // Skip `arg1` fields, each ended by a NUL, a newline or a `\n` / `\N`
        // escape.
        for (; arg1 > 0; str++) {
            c = *str;
            if (c == '\0' || c == '\n' || (c == 'n' && str[-1] == '\\') || (c == 'N' && str[-1] == '\\')) {
                arg1--;
            }
        }
    }
    return str;
}

s32 Gp_NthRelatedId(McItemScan* arg0, s32 arg1, s32 arg2)
{
    McItemRec*    table;
    s32           idx;
    s32           i;
    PlayerStatus* cfg;

    table = _gpScanTable(arg0);
    idx   = arg0->firstRow;
    cfg   = &Player_Status;
    while (arg1 >= 0) {
        if ((u8)(table[idx].itemId + 0x80) < 0x20) {
            if (arg2 == 0) {
                arg1--;
            } else {
                for (i = 0; i < 3; i++) {
                    if (Gp_RelatedQty0[table[idx].itemId - 0x80].related[i] == arg2) {
                        if (table[idx].attachSlot > 0 || cfg->weapon == table[idx].itemId - 0x7F) {
                            arg1--;
                        }
                        break;
                    }
                }
                for (i = 0; i < 3; i++) {
                    if (Gp_RelatedQty1[table[idx].itemId - 0x80].related[i] == arg2) {
                        if (table[idx].attachSlot > 0 || cfg->weapon == table[idx].itemId - 0x7F) {
                            arg1--;
                        }
                        break;
                    }
                }
            }
        }
        idx++;
    }
    idx--;
    return table[idx].itemId;
}

void Gp_RefreshItemRow(McItemRec* arg0)
{
    u8           item;
    McItemSlot*  slot;
    register s32 found asm("a3");
    s32          i;
    GpItemMap*   p;
    s32          inRange;

    if (arg0->attachSlot <= 0) {
        return;
    }

    inRange          = (u8)(arg0->itemId + 0x80) < 0x20;
    arg0->attachSlot = 0;
    if (!inRange) {
        return;
    }

    item = arg0->itemId;
    if (item == Player_Status.weapon + 0x7F) {
        return;
    }

    if ((u32)(item - 0x80) >= 0x20) {
        return;
    }

    found = 0;
    slot  = &((McItemSlot*)((s32)Mc_SaveData[0].weaponItems - 0x400))[item];
    for (i = found, p = Gp_ItemMaps; i < 8; i++, p++) {
        if (item == p->field_1) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
        slot->ammoId  = 0;
        slot->ammoQty = 0;
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
        if (slot->attachId != 0xFF) {
            slot->attachId = 0;
        }
        slot->attachQty = 0;
    }
}

void func_800B92CC(void)
{
    switch (GP_LOC_WORD(Mc_SaveData[0].at4.loc) & GP_LOC_STAGE_AREA) {
        case GP_LOC_KEY(1, 1, 0, 0):
            func_80180804();
            break;
        case GP_LOC_KEY(1, 15, 0, 0):
            func_8017EA68();
            break;
        case GP_LOC_KEY(1, 19, 0, 0):
            func_80181468();
            break;
        case GP_LOC_KEY(2, 1, 0, 0):
            func_8017EA90();
            break;
        case GP_LOC_KEY(2, 17, 0, 0):
            func_8017E9E8();
            break;
        case GP_LOC_KEY(2, 27, 0, 0):
            func_80181364();
            break;
        case GP_LOC_KEY(3, 1, 0, 0):
            func_8017E9F8();
            break;
        case GP_LOC_KEY(3, 17, 0, 0):
            func_8017EAE0();
            break;
        case GP_LOC_KEY(3, 27, 0, 0):
            func_8018138C();
            break;
        case GP_LOC_KEY(4, 6, 0, 0):
            func_8017EA78();
            break;
        case GP_LOC_KEY(4, 16, 0, 0):
            func_8017EB2C();
            break;
        case GP_LOC_KEY(4, 20, 0, 0):
            func_8017EDE8();
            break;
        case GP_LOC_KEY(4, 31, 0, 0):
            func_8017EAB4();
            break;
        case GP_LOC_KEY(4, 41, 0, 0):
            func_8017EA64();
            break;
        case GP_LOC_KEY(4, 47, 0, 0):
            func_8017EC04();
            break;
        case GP_LOC_KEY(5, 22, 0, 0):
            func_8017EAC4();
            break;
        case GP_LOC_KEY(5, 28, 0, 0):
            func_8017EA60();
            break;
        case GP_LOC_KEY(2, 30, 0, 0):
            func_8017EA58();
            break;
        case GP_LOC_KEY(3, 30, 0, 0):
            func_8017EA74();
            break;
    }
}

/// Takes `n` of item `item` from the table window `scan`, or all of it when `n`
/// is negative, emptying the row once none is left. Inline form of
/// `Gp_ConsumeScanQty`.
static inline void _gpConsumeScanQty(McItemScan* scan, s32 item, s32 n)
{
    McItemRec* table;
    s32        qty;
    s32        i;
    s32        left;

    table = _gpScanTable(scan);
    qty   = 0;
    for (i = scan->firstRow; i < scan->firstRow + scan->rowCount; i++) {
        if (table[i].itemId == item) {
            qty = table[i].qty;
            break;
        }
    }
    if (i != scan->firstRow + scan->rowCount) {
        if (n < 0) {
            n = qty;
        }
        left = qty - n;
        if (left < 0) {
            left = 0;
        }
        if (left == 0) {
            table[i].itemId     = 0;
            table[i].qty        = 0;
            table[i].attachSlot = 0;
        } else {
            table[i].qty = left;
        }
    }
}

/// Inline form of `Gp_RemoveItem`: clears the record `arg1` outright when it
/// holds an item below 0xA0, otherwise takes `arg2` of its item from `arg0`.
static __inline void func_800B996C_RemoveItem(McItemScan* arg0, McItemRec* arg1, s32 arg2)
{
    s32 item;

    item = arg1->itemId;
    if (item < 0xA0) {
        arg1->itemId     = 0;
        arg1->qty        = 0;
        arg1->attachSlot = 0;
    } else {
        _gpConsumeScanQty(arg0, item, arg2);
    }
}

/// Level of mod item `item` (0x60-0x7F): its base level plus the bonus
/// accumulated in the save, capped at 10. Any other item has level 0.
static inline s32 _gpGetModLevel(s32 item)
{
    s32         ret;
    s32         idx;
    GpItemAttr* p;

    idx = item - 0x60;
    ret = 0;
    if ((u32)idx < 0x20) {
        p    = &Gp_ItemAttrs[item];
        ret  = p->field_5;
        ret += Mc_SaveData[0].itemLevelBonus[idx];
        if (ret >= 0xB) {
            ret = 0xA;
        }
    }
    return ret;
}

/// Draws the prompt `str` at (x, y), followed on the same line by the
/// highlighted name of `item`.
static inline void _gpDrawPromptItem(UiObject* obj, s32 x, s32 y, u8* str, s32 item, s32 color, s32 one)
{
    s32 width;

    Text_DrawPrompt(obj, x, y, str, color, one, 0);
    width = Text_MeasureWidth(str) + 4;
    Text_DrawPrompt(obj, x + width, y, Gp_GetItemText(item, 0, 0), 0x37A78, one, 0);
}

void Gp_UiBoostAttach(UiObject* arg0, Task* arg1)
{
    s32          item;
    s32          width;
    s32          other;
    s32          saved;
    s32          x;
    s32          y;
    s32          color;
    register s32 row asm("s1");
    s32          one;

    item = Player_Status.armor + 0x5F;
    if (arg1->state == 0) {
        arg1->status = 0xFF;
        if (_gpGetModLevel(item) < 0xA) {
            Mc_SaveData[0].itemLevelBonus[item - 0x60]++;
        } else {
            arg1->status = 0x1A;
        }
        if (arg1->status == 0xFF) {
            width = Text_MeasureWidth(Gp_GetItemText(item, 0, 0)) + Text_MeasureWidth(Gp_StrMore) + 4;
            other = Text_MeasureWidth(Gp_StrAttachAvail);
            if (width < other) {
                width = other;
            }
            Ui_UpdateLayoutSize((UiPanel*)arg0, width + 5, Ui_Scale15(2) + 1);
            ((UiPanel*)arg0)->field_C.x = (-((UiPanel*)arg0)->field_C.w) >> 1;
            ((UiPanel*)arg0)->field_C.y = ((-((UiPanel*)arg0)->field_C.h) >> 1) - 0x14;
            func_800B996C_RemoveItem(&Mc_SaveData[0].carriedItems, Gp_SelItemRec, 1);
            arg1->killCountdown = 0xBC;
            arg1->state++;
        }
    }
    if (arg1->status != 0xFF) {
        saved           = arg1->spawnArg1;
        arg1->spawnArg1 = arg1->status;
        Gp_NoticePanelTask(arg1);
        arg1->spawnArg1 = saved;
        return;
    }

    x = arg0->field_1C + 2;
    y = (s16)arg0->field_18;
    Ui_DrawText((UiPanel*)arg0, Gp_StrNotice2);
    color = 0x606060;
    one   = 1;
    row   = y + 0xF;
    _gpDrawPromptItem(arg0, x, row, Gp_StrMore, item, color, one);
    Text_DrawPrompt(arg0, x, y + 0x1E, Gp_StrAttachAvail, color, one, 0);

    if (arg0->status == one) {
        arg1->killCountdown--;
        if ((arg1->killCountdown <= 0) || (Pad_CheckButtons(0, one, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
            arg0->field_2E      = 9;
            arg1->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            arg0->field_2E      = -1;
            arg1->killCountdown = 0x7FFF;
        }
    }
}

void Gp_UiBoostMp(UiObject* arg0, Task* arg1)
{
    PlayerStatus* cfg;
    McSaveData*   save;
    s32           saved;

    if (arg1->state == 0) {
        cfg                 = &Player_Status;
        Gp_HpMpWork.field_0 = cfg->hp;
        save                = &Mc_SaveData[0];
        Gp_HpMpWork.field_4 = cfg->mp;
        if (save->mpBonus < 0xFA) {
            save->mpBonus = save->mpBonus + 1;
        }
        Gp_RecalcMaxMp();
        cfg->mp = cfg->mpMax;
        func_800B996C_RemoveItem(0, Gp_SelItemRec, 1);
        Ui_SpawnFromDesc(&Gp_BoostPanelDesc, 0, 0, 1, arg0);
    }
    saved           = arg1->spawnArg1;
    arg1->spawnArg1 = 0x1D;
    Gp_NoticePanelTask(arg1);
    arg1->spawnArg1 = saved;
}

void Gp_UiBoostHp(UiObject* arg0, Task* arg1)
{
    PlayerStatus* cfg;
    McSaveData*   save;
    s32           saved;
    s32           hp;
    u16           val;

    if (arg1->state == 0) {
        cfg                 = &Player_Status;
        hp                  = cfg->hp;
        Gp_HpMpWork.field_0 = hp;
        save                = &Mc_SaveData[0];
        Gp_HpMpWork.field_4 = cfg->mp;
        if (save->hpBonus < 0xFA) {
            save->hpBonus = save->hpBonus + 5;
        }
        val        = Gp_StatRows[save->gameMode].base.half;
        cfg->hpMax = val;
        val       += save->hpBonus;
        cfg->hpMax = val;
        if (cfg->armor != 0) {
            val       += Gp_ModStatAttrs[cfg->armor - 1].field_4;
            cfg->hpMax = val;
        }
        if (cfg->hpMax >= 0xFB) {
            cfg->hpMax = 0xFA;
        }
        if (cfg->hpMax < hp) {
            cfg->hp = cfg->hpMax;
        }
        cfg->hp = cfg->hpMax;
        func_800B996C_RemoveItem(0, Gp_SelItemRec, 1);
        Ui_SpawnFromDesc(&Gp_BoostPanelDesc, 0, 0, 1, arg0);
    }
    saved           = arg1->spawnArg1;
    arg1->spawnArg1 = 0x1C;
    Gp_NoticePanelTask(arg1);
    arg1->spawnArg1 = saved;
}

static __inline__ s32 Gp_HasStockedItemInline(s32 arg0)
{
    McItemScan* scan;
    McItemRec*  table;
    s32         i;
    s32         ret;
    s32         count;

    scan = &Mc_SaveData[0].carriedItems;
    ret  = 0;
    switch (scan->table) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData[0].itemRows;
            break;
    }
    i      = 0;
    table += scan->firstRow;
    count  = scan->rowCount;
    for (; i < count; i++) {
        if (table->attachSlot > 0) {
            if (table->itemId == arg0) {
                ret = 1;
                break;
            }
        }
        table++;
    }
    return ret;
}

s32 func_800B9D80(s32 arg0)
{
    PlayerStatus* cfg;
    GpItemAttr*   attr;
    s32           flags;
    s32           ret;
    s32           stateA;
    s32           stateB;

    ret    = 0;
    flags  = 0;
    stateA = 0;
    stateB = 0;
    cfg    = &Player_Status;
    if (cfg->armor != 0) {
        attr  = &Gp_ItemAttrs[cfg->armor + 0x5F];
        flags = attr->flags;
    }
    if ((Gp_StateC08.field_14 > 0) || (Gp_StateC08.field_17 != 0)) {
        stateA = 1;
    }
    if ((Gp_StateC08.field_14 > 0) || (Gp_StateC08.field_16 != 0)) {
        stateB = 1;
    }

    switch (arg0) {
        case 0x101:
            if (Gp_HasStockedItemInline(0x3F) || stateA) {
                ret = 1;
            }
            break;
        case 0x102:
            if (stateA || (flags & 0x1000)) {
                ret = 1;
            }
            break;
        case 0x104:
            if (stateA || (flags & 2)) {
                ret = 1;
            }
            break;
        case 0x108:
            ret = Gp_HasStockedItemInline(0xB);
            if (stateB || (flags & 0x20)) {
                ret = 1;
            }
            break;
        case 0x110:
            break;
        case 0x120:
            ret = Gp_HasStockedItemInline(0xE);
            if (stateB || (flags & 0x200)) {
                ret = 1;
            }
            break;
        case 0x140:
            if (stateB || Gp_HasStockedItemInline(0xE)) {
                ret = 1;
            }
            break;
        case 0x200:
            if (flags & 0x10) {
                ret = 1;
            }
            break;
        case 0x400:
            if (flags & 1) {
                ret = 1;
            }
            break;
        case 0x800:
            if (flags & 4) {
                ret = 1;
            }
            break;
        case 0x1000:
            if (flags & 0x100) {
                ret = 1;
            }
            break;
        case 0x2000:
            if (flags & 8) {
                ret = 1;
            }
            break;
        case 0x4000:
            if (flags & 0x40) {
                ret = 1;
            }
            break;
        case 0x8000:
            if (flags & 0x80) {
                ret = 1;
            }
            break;
        case 0x10000:
            ret = Gp_HasStockedItemInline(0x36);
            break;
        case 0x20000:
            ret = Gp_HasStockedItemInline(0x39);
            break;
        case 0x40000:
            ret = Gp_HasStockedItemInline(0x38);
            break;
        case 0x80000:
            ret = Gp_HasStockedItemInline(0x37);
            break;
        case 0x100000:
            if (Gp_HasStockedItemInline(0x40) || (flags & 1)) {
                ret = 1;
            }
            break;
    }
    return ret;
}

/// Empties the ammunition and attachment of weapon item `item`, the same clear
/// `Gp_ClearEquipSlot` performs.
static inline void _gpClearEquipSlot(s32 item)
{
    McItemSlot* slot;
    s32         found;
    s32         i;

    if ((u32)(item - 0x80) >= 0x20) {
        return;
    }

    found = 0;
    slot  = &Mc_SaveData[0].weaponItems[item - 0x80];
    for (i = 0; i < 8; i++) {
        if (item == Gp_ItemMaps[i].field_1) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
        slot->ammoId  = 0;
        slot->ammoQty = 0;
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
        if (slot->attachId != 0xFF) {
            slot->attachId = 0;
        }
        slot->attachQty = 0;
    }
}

/// Zeroes every row of the table window `scan`. Shared by `Gp_ClearScanItems`
/// and the callers that clear a window in line.
static inline void _gpClearScanItems(McItemScan* scan)
{
    McItemRec* table;
    s32        i;
    s32        row;

    switch (scan->table) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData[0].itemRows;
            break;
    }
    for (i = 0, row = scan->firstRow; i < scan->rowCount; i++, row++) {
        table[row].itemId     = 0;
        table[row].attachSlot = 0;
        table[row].qty        = 0;
    }
}

void Gp_ResetInventory(void)
{
    PlayerStatus* status;
    s32           i;
    s32           j;

    status = &Player_Status;
    if (status->weapon != 0) {
        _gpClearEquipSlot(status->weapon + 0x7F);
        status->weapon = 0;
    }

    _gpClearScanItems(&Gp_DefaultScan);
    Mc_SaveData[0].carriedItems = Gp_DefaultScan;
    Gp_AddItem(&Mc_SaveData[0].carriedItems, 0x6C, 1);
    Gp_EquipMod(0x6C);

    Player_Status.hp = Player_Status.hpMax;
    Player_Status.mp = Player_Status.mpMax;
    Gp_ApplyItemMap();

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 3; j++) {
            Gp_DebugAttachLevels[j + i * 3] = 0;
        }
    }
    Gp_DebugAttachLevels[0] = 1;

    Gp_StateC08.field_5 = 0;
    Gp_StateC08.field_B = 0;
}

/// Recomputes `Player_Status.hpMax` from the game mode's base, the save's HP
/// bonus and the equipped armour, clamping current HP to it.
static inline void _gpRecalcMaxHp(void)
{
    PlayerStatus* cfg;
    McSaveData*   save;
    GpStatRow*    table;
    u16           val;

    cfg        = &Player_Status;
    table      = Gp_StatRows;
    save       = &Mc_SaveData[0];
    val        = table[save->gameMode].base.half;
    cfg->hpMax = val;
    val       += save->hpBonus;
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
}

/// Points the carried-items window at the first `count` rows of the save's
/// own item table, as `Gp_SetPlayerScan` does.
static inline void _gpSetPlayerScan(s32 count)
{
    McSaveData* p;

    p                        = &Mc_SaveData[0];
    p->carriedItems.firstRow = 0;
    p->carriedItems.rowCount = count;
    p->carriedItems.table    = 0;
}

void Gp_ClearInventory(void)
{
    PlayerStatus* status;
    McItemScan*   scan;
    McItemRec*    rec;
    s32           i;

    status = &Player_Status;
    if (status->weapon != 0) {
        _gpClearEquipSlot(status->weapon + 0x7F);
        status->weapon = 0;
    }

    _gpClearScanItems(&Gp_DefaultScan);
    _gpSetPlayerScan(0x14);
    scan = &Mc_SaveData[0].carriedItems;

    rec = &_gpScanTable(scan)[scan->firstRow];
    for (i = 0; i < scan->rowCount; i++, rec++) {
        if (rec->attachSlot == -1 && (u32)(rec->itemId - 0x60) < 0x20) {
            status->armor = rec->itemId - 0x5F;
            _gpRecalcMaxHp();
            Gp_RecalcMaxMp();
            break;
        }
    }

    Gp_StateC08.field_B = 0;
    Gp_StateC08.field_5 = 0;
    Player_Status.hp    = Player_Status.hpMax;
    Player_Status.mp    = Player_Status.mpMax;
    Gp_ApplyItemMap();
}

static void Gp_InitModeEquip(void)
{
    PlayerStatus* cfg;
    McItemScan*   scan;
    McItemRec*    tmp;
    McItemRec*    table;
    McItemRec*    rec;
    s32           i;
    s32           acc;
    s32           count;
    s32           start;
    s32           limit;
    s32           off;
    s32           item;
    McItemSlot*   slots;
    u8            slotItem;

    cfg = &Player_Status;
    acc = 0;
    if (cfg->weapon == 0) {
        scan = &Mc_SaveData[0].carriedItems;
        item = 0x81;
        switch (scan->table) {
            case 2:
                tmp = Gp_ItemTable2;
                break;
            case 1:
                tmp = Gp_ItemTable1;
                break;
            default:
                tmp = Mc_SaveData[0].itemRows;
                break;
        }
        table = tmp;
        i     = 0;
        count = scan->rowCount;
        start = scan->firstRow;
        if (count != 0) {
            limit = count;
            off   = start << 2;
            rec   = (McItemRec*)(off + (s32)table);
            do {
                if (rec->itemId == item) {
                    acc += rec->qty;
                }
                i++;
                rec++;
            } while (i < limit);
        }
        if (acc != 0) {
            Gp_EquipHeld(0x81);
        }
    }
    if (cfg->weapon == 2) {
        slots    = (McItemSlot*)((s32)Mc_SaveData[0].weaponItems - 0x400);
        slotItem = slots[0x81].ammoId;
        if ((slotItem == 0) || (slotItem == 0xA0)) {
            Gp_EquipRelatedItem(&Mc_SaveData[0].carriedItems, 0x81, 0xA0, -1);
        }
    }
}

/// Writes the low two bits of each record's `field_6` into the 2-bit slot
/// `field_0` of `dest`, for every record list in `table`. Shared by
/// `Gp_ApplyBit2List` and `Gp_ApplyBit2Bank`.
static inline void _gpApplyBit2List(GpBit2List* table, u32* dest)
{
    GpBit2Rec* rec;
    u32*       p;
    u32        mask;

    if (table == NULL) {
        return;
    }
    rec = table->field_0;
    if (rec == (GpBit2Rec*)-1) {
        return;
    }
    do {
        if (rec != NULL) {
            for (; rec->field_0 != 0xFFFF; rec++) {
                mask = 3 << ((rec->field_0 & 0xF) * 2);
                p    = &dest[rec->field_0 >> 4];
                *p  &= ~mask;
                mask = (rec->field_6 & 3) << ((rec->field_0 & 0xF) * 2);
                *p  |= mask;
            }
        }
        table++;
        rec = table->field_0;
    } while (rec != (GpBit2Rec*)-1);
}

void Gp_ApplyBit2Bank(s32 arg0)
{
    GpBit2List* table;
    u32*        dest;

    table = Gp_Bit2Banks[arg0].field_0;
    dest  = Gp_Bit2Banks[arg0].field_4;
    if (arg0 == 3) {
        return;
    }
    _gpApplyBit2List(table, dest);
}

void Gp_SetCurBit2Flag(s32 arg0, u8 arg1)
{
    s32  shift;
    u32  mask;
    u32* p;
    s32  stage;

    shift = (arg0 & 0xF) * 2;
    mask  = 3 << shift;
    stage = Mc_SaveData[0].at4.loc.stage;
    p     = &Gp_Bit2Banks[stage].field_4[arg0 >> 4];
    *p   &= ~mask;
    mask  = arg1 << shift;
    *p   |= mask;
}

void Gp_ClearScanItems(McItemScan* scan)
{
    _gpClearScanItems(scan);
}

McItemRec* Gp_GiveItem(McItemScan* arg0, s32 arg1, s32 arg2)
{
    return Gp_AddItem(arg0, arg1, arg2);
}

s32 Gp_RemoveItem(McItemScan* arg0, McItemRec* arg1, s32 arg2)
{
    McItemRec* table;
    s32        item;
    s32        qty;
    s32        i;

    item = arg1->itemId;
    if (item < 0xA0) {
        arg1->itemId     = 0;
        arg1->qty        = 0;
        arg1->attachSlot = 0;
    } else {
        table = _gpScanTable(arg0);
        qty   = 0;
        for (i = arg0->firstRow; i < arg0->firstRow + arg0->rowCount; i++) {
            if (table[i].itemId == item) {
                qty = table[i].qty;
                break;
            }
        }
        if (i != arg0->firstRow + arg0->rowCount) {
            if (arg2 < 0) {
                arg2 = qty;
            }
            arg2 = qty - arg2;
            if (arg2 < 0) {
                arg2 = 0;
            }
            if (arg2 == 0) {
                table[i].itemId     = 0;
                table[i].qty        = 0;
                table[i].attachSlot = 0;
            } else {
                table[i].qty = arg2;
            }
        }
    }
    return 0;
}

static void Gp_ClearCollectedBits(void)
{
    s32  i;
    s32* p;

    p = Mc_SaveData[0].collectedBits;
    for (i = 3; i >= 0; i--) {
        *p++ = 0;
    }
}

void Gp_SetCollectedBit(s32 arg0)
{
    s32* p;
    s32  bit;

    p    = Mc_SaveData[0].collectedBits;
    bit  = arg0 & 0x7F;
    p   += bit / 32;
    bit %= 32;
    *p  |= 1 << bit;
    if ((arg0 & 0x7F) == 0x19) {
        Gp_PlayTimeMark = Mc_SaveData[0].playTime;
    }
}

void Gp_ClearCollectedBit(s32 arg0)
{
    s32* p;

    p     = Mc_SaveData[0].collectedBits;
    arg0 &= 0x7F;
    p    += arg0 / 32;
    arg0 %= 32;
    *p   &= ~(1 << arg0);
}

s32 Gp_CountCollectedBits(void)
{
    s32  count;
    s32* p;
    s32  i;
    s32  bit;
    s32  word;
    s32  one;

    p     = Mc_SaveData[0].collectedBits;
    count = 0;
    one   = 1;
    for (i = 3; i >= 0; i--) {
        bit  = 0;
        word = *p;
        do {
            if (word & (one << bit)) {
                count++;
            }
            bit++;
        } while (bit < 32);
        p++;
    }
    return count;
}

s32 Gp_CountScanItems(McItemScan* arg0)
{
    McItemRec* tmp;
    McItemRec* table;
    McItemRec* rec;
    s32        i;
    s32        ret;
    s32        count;
    s32        start;
    s32        limit;
    s32        off;

    switch (arg0->table) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData[0].itemRows;
            break;
    }
    table = tmp;
    i     = 0;
    count = arg0->rowCount;
    start = arg0->firstRow;
    ret   = i;
    if (count != 0) {
        limit = count;
        off   = start << 2;
        rec   = (McItemRec*)(off + (s32)table);
        do {
            if (rec->itemId != 0) {
                ret++;
            }
            i++;
            rec++;
        } while (i < limit);
    }
    return ret;
}

McItemSlot* Gp_GetItemSlot(s32 arg0)
{
    return &((McItemSlot*)((s32)Mc_SaveData[0].weaponItems - 0x400))[arg0];
}

s32 Gp_CountEquippedRelated(McItemScan* arg0, s32 arg1)
{
    McItemRec*  table;
    McItemSlot* slot;
    s32         count;
    s32         i;
    s32         end;
    s32         itemId;

    table = Gp_GetItemTable(arg0);
    count = 0;
    if ((u32)(arg1 - 0xA0) < 0x20) {
        i   = arg0->firstRow;
        end = i + arg0->rowCount;
        if (i < end) {
            for (; i < arg0->firstRow + arg0->rowCount; i++) {
                itemId = table[i].itemId;
                if ((u32)(itemId - 0x80) < 0x20) {
                    slot = gpItemSlot(itemId);
                    if (slot->ammoId == arg1) {
                        count += slot->ammoQty;
                    }
                    if (slot->attachId == arg1) {
                        count += slot->attachQty;
                    }
                }
            }
            return count;
        }
    }
    return count;
}

void Gp_ClearEquipSlot(s32 arg0)
{
    McItemSlot* slot;
    s32         found = 0;
    s32         i;

    if ((u32)(arg0 - 0x80) >= 0x20) {
        return;
    }

    slot = &Mc_SaveData[0].weaponItems[arg0 - 0x80];
    for (i = 0; i < 8; i++) {
        if (arg0 == Gp_ItemMaps[i].field_1) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
        slot->ammoId  = 0;
        slot->ammoQty = 0;
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
        if (slot->attachId != 0xFF) {
            slot->attachId = 0;
        }
        slot->attachQty = 0;
    }
}

void Gp_ClearEquipSlotSel(s32 arg0, s32 arg1)
{
    McItemSlot* slot;
    s32         found = 0;
    s32         i;

    if ((u32)(arg0 - 0x80) >= 0x20) {
        return;
    }

    slot = &Mc_SaveData[0].weaponItems[arg0 - 0x80];
    for (i = 0; i < 8; i++) {
        if (arg0 == Gp_ItemMaps[i].field_1) {
            found = 1;
            break;
        }
    }

    if (arg1 != 2) {
        if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
            slot->ammoId  = 0;
            slot->ammoQty = 0;
        }
    }

    if (arg1 != 1) {
        if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
            if (slot->attachId != 0xFF) {
                slot->attachId = 0;
            }
            slot->attachQty = 0;
        }
    }
}

s32 Gp_ScanStackQty(McItemScan* arg0, s32 arg1)
{
    s32        index;
    s32        ret;
    McItemRec* table;

    index = arg0->firstRow;
    table = Gp_GetItemTable(arg0);
    if ((u32)(arg1 - 0xA0) < 0x20) {
        ret = (s16)Gp_FindScanQty(table, arg0, &index, arg1);
    } else {
        ret = 0;
    }
    return ret;
}

void Gp_ConsumeScanQty(McItemScan* arg0, s32 arg1, s32 arg2)
{
    McItemRec* table;
    s32        qty;
    s32        i;

    table = _gpScanTable(arg0);
    qty   = 0;
    for (i = arg0->firstRow; i < arg0->firstRow + arg0->rowCount; i++) {
        if (table[i].itemId == arg1) {
            qty = table[i].qty;
            break;
        }
    }
    if (i != arg0->firstRow + arg0->rowCount) {
        if (arg2 < 0) {
            arg2 = qty;
        }
        arg2 = qty - arg2;
        if (arg2 < 0) {
            arg2 = 0;
        }
        if (arg2 == 0) {
            table[i].itemId     = 0;
            table[i].qty        = 0;
            table[i].attachSlot = 0;
        } else {
            table[i].qty = arg2;
        }
    }
}

s32 Gp_FillRelated(s32 arg0, s32 arg1)
{
    McItemSlot* slot;
    McItemSlot* alt;
    s32         ret;

    slot = &((McItemSlot*)((s32)Mc_SaveData[0].weaponItems - 0x400))[arg0];
    alt  = slot;
    if (arg1 != 0) {
        ret = Gp_EquipRelatedItem(&Mc_SaveData[0].carriedItems, arg0, slot->attachId, -1);
    } else {
        ret = Gp_EquipRelatedItem(&Mc_SaveData[0].carriedItems, arg0, alt->ammoId, -1);
    }
    return ret;
}

s32 Gp_UnequipRelated(s32 arg0, s32 arg1)
{
    McItemSlot* slot;
    McItemSlot* alt;
    s32         ret;

    slot = &((McItemSlot*)((s32)Mc_SaveData[0].weaponItems - 0x400))[arg0];
    alt  = slot;
    if (arg1 == 0) {
        ret = Gp_EquipRelatedItem(&Mc_SaveData[0].carriedItems, arg0, slot->ammoId, 0);
    } else {
        ret = Gp_EquipRelatedItem(&Mc_SaveData[0].carriedItems, arg0, alt->attachId, 0);
    }
    return ret == 0;
}

s32 Gp_GetCurBit2Flag(s32 arg0)
{
    register u32* p asm("v1");
    u32           word;
    s32           shift;

    p     = Gp_Bit2Banks[gGameSession->at4.loc.stage].field_4;
    p    += arg0 >> 4;
    shift = (arg0 & 0xF) * 2;
    word  = *p;
    CLOBBER_REG(a1);
    return (word & (3 << shift)) >> shift;
}

s32 Gp_HasCollectedBit(s32 arg0)
{
    s32* p;
    s32  val;

    p     = Mc_SaveData[0].collectedBits;
    arg0 &= 0x7F;
    p    += arg0 / 32;
    arg0 %= 32;
    val   = *p & (1 << arg0);
    return val != 0;
}

McItemRec* Gp_GetItemTable(McItemScan* arg0)
{
    switch (arg0->table) {
        case 2:
            return Gp_ItemTable2;
        case 1:
            return Gp_ItemTable1;
        default:
            return Mc_SaveData[0].itemRows;
    }
}

s32 Gp_ScanIndexOf(McItemScan* arg0, McItemRec* arg1)
{
    McItemRec*   table;
    register s32 i asm("a2");
    s32          ret;

    switch (arg0->table) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData[0].itemRows;
            break;
    }

    ret    = -1;
    table += arg0->firstRow;
    for (i = 0; i < arg0->rowCount; i++) {
        if (table == arg1) {
            ret = i;
            break;
        }
        table++;
    }
    return ret;
}

McItemRec* Gp_GetScanSlot(McItemScan* arg0, s32 arg1, s32 arg2)
{
    McItemRec* table;

    switch (arg0->table) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData[0].itemRows;
            break;
    }
    return &table[arg0->firstRow + arg1];
}

static s32 Gp_GetScanItemId(McItemScan* arg0, s32 arg1)
{
    McItemRec* table;
    McItemRec* rec;

    switch (arg0->table) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData[0].itemRows;
            break;
    }
    rec = &table[arg0->firstRow + arg1];
    return rec->itemId;
}

s32 Gp_NthCollectedId(s32 arg0, s32 arg1)
{
    u16* p;
    s32  ret;
    s32* bits;
    s32  bit;
    s32  item;
    s32  one;

    p   = Gp_CollectedIds;
    ret = 0;
    if (*p != 0xFFFF) {
        do {
            item  = *p;
            bits  = Mc_SaveData[0].collectedBits;
            one   = 1;
            bit   = item & 0x7F;
            bits += bit / 32;
            bit  %= 32;
            if (*bits & (one << bit)) {
                arg0--;
                p++;
                if (arg0 >= 0) {
                    continue;
                }
                ret = item;
                break;
            }
            p++;
        } while (*p != 0xFFFF);
    }
    return ret;
}

s32 Gp_SumScanQty(McItemScan* arg0, s32 arg1)
{
    McItemRec* tmp;
    McItemRec* table;
    McItemRec* rec;
    s32        i;
    s32        acc;
    s32        count;
    s32        start;
    s32        limit;
    s32        off;

    if (arg1 >= 0x100) {
        return Gp_HasCollectedBit(arg1);
    }

    acc = 0;
    switch (arg0->table) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData[0].itemRows;
            break;
    }
    table = tmp;
    i     = 0;
    count = arg0->rowCount;
    start = arg0->firstRow;
    if (count != 0) {
        limit = count;
        off   = start << 2;
        rec   = (McItemRec*)(off + (s32)table);
        do {
            if (rec->itemId == arg1) {
                acc += rec->qty;
            }
            i++;
            rec++;
        } while (i < limit);
    }
    return acc;
}

static void func_800BB7B4(Task* arg0)
{
    arg0->extra.tmd->flags = 0;
}

void Gp_SetItemSeenBit(s32 arg0, s32 arg1)
{
    McSaveData* p;
    s32         word;
    s32         bit;

    word = arg0 / 32;
    bit  = 1 << (arg0 % 32);
    if ((u32)arg0 >= 0x180) {
        return;
    }
    if (arg1 == 0) {
        p                      = &Mc_SaveData[0];
        p->itemSeenBits[word] &= ~bit;
        return;
    }
    p                      = &Mc_SaveData[0];
    p->itemSeenBits[word] |= bit;
}

static void Gp_ApplyBit2List(GpBit2List* table, u32* dest)
{
    _gpApplyBit2List(table, dest);
}

void Gp_SetBit2Flag(s32 arg0, u8 arg1, s32 arg2)
{
    s32          shift;
    u32          mask;
    u32*         p;
    u32          nmask;
    register s32 temp asm("v0");

    shift = (arg0 & 0xF) * 2;
    USE_REG(shift);
    temp  = 3;
    mask  = temp << shift;
    p     = Gp_Bit2Banks[arg2].field_4;
    p    += arg0 >> 4;
    nmask = ~mask;
    temp  = *p;
    mask  = arg1 << shift;
    *p    = (temp & nmask) | mask;
}

s32 Gp_GetRelatedQty(s32 arg0, s32 arg1)
{
    s32 ret;

    arg0 -= 0x80;
    ret   = 0;
    if ((u32)arg0 < 0x20) {
        if (arg1 == 0) {
            ret = Gp_RelatedQty0[arg0].field_0;
        } else {
            ret = Gp_RelatedQty1[arg0].field_0;
        }
    }
    return ret;
}

static s32 Gp_GetBit2Flag(GpAreaKey* arg0, s32 arg1)
{
    register u32* p asm("v1");
    u32           word;
    s32           shift;

    p     = Gp_Bit2Banks[arg0->stage].field_4;
    p    += arg1 >> 4;
    shift = (arg1 & 0xF) * 2;
    word  = *p;
    USE_REG(arg0);
    return (word & (3 << shift)) >> shift;
}

void Gp_SavePlayerPos(void)
{
    GpCoord*      coord;
    PlayerPos*    p;
    s32           angle;
    s32           temp;
    PlayerStatus* cfg;
    McSaveData*   save;

    coord  = (gameGetPtrSlot(3))->extra.tmd->coords;
    temp   = (u16)coord->coord.t[0];
    p      = &Player_Status.pos;
    p->x   = temp;
    p->y   = (u16)coord->coord.t[1];
    p->z   = (u16)coord->coord.t[2];
    angle  = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    p->yaw = angle;
    if ((s16)angle >= 0x801) {
        p->yaw = angle - 0x1000;
    } else if ((s16)angle < -0x800) {
        p->yaw = angle + 0x1000;
    }
    cfg             = &Player_Status;
    save            = &Mc_SaveData[0];
    save->playerExp = cfg->exp;
    save->playerBp  = cfg->bp;
}

static GpEnemy* Gp_SpawnAtPlace(GpEnemyDesc* arg0, GpBit2Rec* arg1)
{
    GpEnemy*   enemy;
    Task*      task;
    TmdObject* extra;
    GpCoord*   coord;

    enemy = Gp_SpawnEnemyFromTable(&arg0->field_4, 0, arg0->field_0, NULL);
    if (enemy != NULL) {
        task = enemy->task;
        if (task->spawnType != 0) {
            extra               = task->extra.tmd;
            coord               = extra->coords;
            enemy->placeKey     = arg1->field_0 | (arg1->field_4 << 8);
            enemy->workType     = arg1->field_2;
            coord->coord.t[0]   = arg1->field_8;
            coord->coord.t[1]   = arg1->field_A;
            coord->coord.t[2]   = arg1->field_C;
            coord->param.rot.vy = arg1->field_E;
            if (coord->param.rot.vy != 0) {
                Gfx_RotMatrixY(&coord->coord, (s16)arg1->field_E, 1);
            }
            coord->flg = 0;
        }
    }
    return enemy;
}

static void func_800BBB54(Task* arg0)
{
    TmdObject* extra;

    extra = arg0->extra.tmd;
    if (arg0->state == 0) {
        extra->flags = 0x88;
        arg0->state += 1;
    }
    if (arg0->state == 1) {
        GpBit2Bank*  banks;
        u32*         p;
        u32*         indexed;
        GameSession* sess;
        s32          id;
        s32          shift;
        u32          word;

        sess    = gGameSession;
        banks   = Gp_Bit2Banks;
        id      = ((GpItemObj8*)arg0->spawnArg2)->field_8;
        p       = banks[sess->at4.loc.stage].field_4;
        indexed = p + (id >> 4);
        shift   = (id & 0xF) * 2;
        word    = *indexed;
        if (((word & (3 << shift)) >> shift) == 2) {
            extra->flags &= 0xFFF7;
            Task_CallExit(arg0);
        }
    }
}

void Gp_WaitItemFlag2(Task* arg0)
{
    TmdObject* extra;

    extra = arg0->extra.tmd;
    if (arg0->state == 0) {
        extra->flags = 8;
        arg0->state += 1;
    }
    if (arg0->state == 1) {
        GpBit2Bank*   banks;
        register u32* p asm("v1");
        GameSession*  sess;
        s32           id;
        s32           shift;
        u32           word;

        sess  = gGameSession;
        banks = Gp_Bit2Banks;
        id    = ((GpItemObj8*)arg0->spawnArg2)->field_8;
        p     = banks[sess->at4.loc.stage].field_4;
        p    += id >> 4;
        shift = (id & 0xF) * 2;
        word  = *p;
        if (((word & (3 << shift)) >> shift) == 2) {
            extra->flags &= 0xFFF7;
            Task_CallExit(arg0);
        }
    }
}

s32 Gp_FindScanQty(McItemRec* arg0, McItemScan* arg1, s32* arg2, s32 arg3)
{
    s32 i;
    s32 ret;
    s32 next;

    i   = *arg2;
    ret = 0;
    if (i < arg1->firstRow + arg1->rowCount) {
    loop:
        if (arg0[i].itemId == arg3) {
            ret = arg0[i].qty;
        } else {
            next  = i + 1;
            *arg2 = next;
            i     = next;
            if (i < arg1->firstRow + arg1->rowCount) {
                goto loop;
            }
        }
    }
    return (s16)ret;
}

s32 Gp_NextMappedSlot(s32 arg0)
{
    McItemScan* scan;
    s32         i;
    GpItemMap*  p;

    scan = &Mc_SaveData[0].carriedItems;
    if ((u32)arg0 >= 8) {
        return -1;
    }
    for (i = arg0; i < 8; i++) {
        p = &Gp_ItemMaps[i];
        if (Gp_SumScanQty(scan, p->field_1)) {
            return i;
        }
    }
    return -1;
}

GpItemMap* Gp_GetItemMap(s32 arg0)
{
    return &Gp_ItemMaps[arg0];
}

s32 Gp_HasMappedItem(void)
{
    s32         found;
    McItemScan* scan;
    s32         i;
    GpItemMap*  p;

    found = 0;
    scan  = &Mc_SaveData[0].carriedItems;
    for (i = 0, p = Gp_ItemMaps; i < 8; i++) {
        if (Gp_SumScanQty(scan, p->field_1)) {
            found = 1;
            break;
        }
        p++;
    }
    return found;
}

static void Gp_ResetAuxSlots(void)
{
    McItemSlot* p;
    s32         i;

    p = Mc_SaveData[0].weaponItems;
    for (i = 0; i < 0x20; i++) {
        p->ammoId    = 0;
        p->ammoQty   = 0;
        p->attachId  = 0xFF;
        p->attachQty = 0;
        if (i == 0x1A) {
            p->attachId  = 0;
            p->attachQty = 0;
        }
        p->field_4 = 0;
        p++;
    }
    Gp_ApplyItemMap();
}

static s32 Gp_SumItemQty(s32 arg0)
{
    McItemScan query;

    memset(&query, 0, sizeof(query));
    query.rowCount = 0xFF;
    return Gp_SumScanQty(&query, arg0);
}

static void Gp_SetPlayerScan(s32 arg0)
{
    McSaveData* p;

    p                        = &Mc_SaveData[0];
    p->carriedItems.firstRow = 0;
    p->carriedItems.rowCount = arg0;
    p->carriedItems.table    = 0;
}

void Gp_SyncHeldRelated(void)
{
    PlayerStatus* p;
    McItemSlot*   slots;
    s32           idx;
    u8            item;

    p = &Player_Status;
    if (p->weapon == 0) {
        p->weaponSlotItem = 0;
    } else {
        slots = (McItemSlot*)((s32)Mc_SaveData[0].weaponItems - 0x400);
        idx   = p->weapon + 0x7F;
        item  = slots[idx].ammoId;
        if (item == 0) {
            p->weaponSlotItem = 0;
        } else {
            p->weaponSlotItem = item + 0x61;
        }
    }
    func_801061F0();
}

static void Gp_InitItemSeenBits(void)
{
    McSaveData* p;
    GpItemDesc* desc;
    u8*         str;
    s32         i;
    s32         count;

    p = &Mc_SaveData[0];
    for (i = 0x5F; i >= 0; i--) {
        p->itemSeenBits[i] = 0;
    }

    i = 0;
    do {
        count = 3;
        if (i < 0x100) {
            desc = &Gp_ItemDescs[i];
        } else {
            desc = &Gp_ItemDescsHi[i];
        }
        str = desc->field_4;
        while (count > 0) {
            if (*str == 0 || *str == 0xA) {
                count--;
            }
            str++;
        }
        if (*str == 0xA) {
            Gp_SetItemSeenBit(i, 1);
        }
        i++;
    } while (i < 0x180);
}

s32 Gp_HasItemSeenBit(s32 arg0)
{
    McSaveData* p;
    s32         word;
    s32         bit;
    s32         val;

    word = arg0 / 32;
    bit  = 1 << (arg0 % 32);
    if ((u32)arg0 >= 0x180) {
        return 1;
    }
    p   = &Mc_SaveData[0];
    val = p->itemSeenBits[word] & bit;
    return val != 0;
}

void Gp_RecalcMaxHp(void)
{
    PlayerStatus* cfg;
    McSaveData*   save;
    GpStatRow*    table;
    u16           val;

    cfg        = &Player_Status;
    table      = Gp_StatRows;
    save       = &Mc_SaveData[0];
    val        = table[save->gameMode].base.half;
    cfg->hpMax = val;
    val       += save->hpBonus;
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
}

void Gp_FillHpMp(void)
{
    PlayerStatus* p;

    p     = &Player_Status;
    p->hp = p->hpMax;
    p->mp = p->mpMax;
}

s32 Gp_GetScanCount(McItemScan* scan)
{
    return scan->rowCount;
}

s32 Gp_ItemSortKey(s32 arg0)
{
    register s32 ret asm("v1");
    s32          idx;

    ret = 0;
    if (arg0 == 0) {
        ret = 0x1000;
    } else if ((u32)(arg0 - 1) < 0x5F) {
        ret = Gp_ItemSortKey0[arg0];
    } else {
        idx = arg0 - 0x60;
        if ((u32)idx < 0x20) {
            ret = Gp_ItemSortKey60[idx];
        } else {
            idx = arg0 - 0x80;
            if ((u32)idx < 0x20) {
                ret = Gp_ItemSortKey80[idx];
            } else {
                idx = arg0 - 0xA0;
                if ((u32)idx < 0x20) {
                    ret = Gp_ItemSortKeyA0[idx];
                }
            }
        }
    }
    if (ret == 0) {
        ret = arg0 + 0x100;
    }
    return ret;
}

void Gp_MarkPlayTime(void)
{
    Gp_PlayTimeMark = Mc_SaveData[0].playTime;
}

static s16 Gp_PlayTimeDelta(void)
{
    u16* p;

    p = &Gp_PlayTimeMark;
    return Mc_SaveData[0].playTime - *p;
}

s32 Gp_AgeFlag119(void)
{
    s32  ret;
    u16* p;

    ret = 0;
    if (Gp_HasCollectedBit(0x119) != 0) {
        p = &Gp_PlayTimeMark;
        if ((s16)(Mc_SaveData[0].playTime - *p) >= 2) {
            Gp_ClearCollectedBit(0x119);
            Gp_SetCollectedBit(0x11A);
            ret = 1;
        }
    }
    return ret;
}

void Gp_AgeFlag119Void(void)
{
    u16* p;

    if (Gp_HasCollectedBit(0x119) != 0) {
        p = &Gp_PlayTimeMark;
        if ((s16)(Mc_SaveData[0].playTime - *p) >= 2) {
            Gp_ClearCollectedBit(0x119);
            Gp_SetCollectedBit(0x11A);
        }
    }
}

s32 Gp_GetModLevel(s32 arg0)
{
    return _gpGetModLevel(arg0);
}

void Gp_TickBoostPanel(Task* arg0)
{
    UiPanel* panel;

    panel = arg0->spawnArg2;
    if (arg0->state == 0) {
        Ui_UpdateLayoutSize(panel, 0xB0, 0x2F);
        panel->field_C.y = -0xC;
        panel->field_C.x = -panel->field_C.w / 2;
        arg0->state++;
    }
    Gp_DrawHpMpStats(panel, 0);
}

s32 Gp_HasStockedItem(s32 arg0)
{
    McItemScan* scan;
    McItemRec*  table;
    s32         i;
    s32         ret;
    s32         count;

    scan = &Mc_SaveData[0].carriedItems;
    ret  = 0;
    switch (scan->table) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData[0].itemRows;
            break;
    }
    i      = 0;
    table += scan->firstRow;
    count  = scan->rowCount;
    for (; i < count; i++) {
        if (table->attachSlot > 0) {
            if (table->itemId == arg0) {
                ret = 1;
                break;
            }
        }
        table++;
    }
    return ret;
}

void Gp_ResetScanDefault(void)
{
    McSaveData* p;

    p               = &Mc_SaveData[0];
    p->carriedItems = Gp_DefaultScan;
}

void func_800BC4BC(void)
{
    Player_Status.field_26 = 1;
    Gp_InitModeEquip();
}

void func_800BC4E4(void)
{
    Player_Status.field_26 = 2;
    Gp_InitModeEquip();
}

s32 Gp_CanMoveItems(void)
{
    McItemScan* src;
    McItemRec*  table;
    s32         row;
    s32         count;
    s32         blocked;
    s32         ret;
    s32         i;

    src     = &Gp_MoveScanSrc;
    ret     = 0;
    table   = Gp_GetItemTable(src);
    row     = src->firstRow;
    count   = Gp_CountScanItems(src + 1);
    blocked = 0; /* nothing sets it, yet the original still tests it */
    if (Gp_CountScanItems(src) > 0) {
        for (i = 0; i < src->rowCount; i++, row++) {
            if (table[row].itemId != 0) {
                /* 0xA0-0xBF items need no new row if the destination already holds one */
                if ((u8)(table[row].itemId + 0x60) < 0x20) {
                    if (Gp_FindItemInScan(table[row].itemId, &Gp_MoveScanDst) == 0) {
                        count++;
                    }
                } else {
                    count++;
                }
            }
        }
        if (blocked == 0 && Gp_MoveScanDst.rowCount >= count) {
            ret = 1;
        }
    }
    return ret;
}

/// "Notice". The byte after the terminator is not zero: the original toolchain
/// left it in the alignment gap.
static const char Gp_StrNotice2[8] = "Notice\0F";
