#include "common.h"

#include <psyq/libgte.h>
#include <psyq/memory.h>

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/4CC.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern u16          Gp_PlayTimeMark;
extern GpItemRec*   Gp_SelItemRec;
extern UiObjectDesc Gp_BoostPanelDesc;
extern u8           Gp_DebugAttachLevels[];
extern char         Gp_StrNotice2[];
extern u8           Gp_StrMore[];
extern u8           Gp_StrAttachAvail[];
extern u8           D_8010D318[];
extern u8           D_8010D320[];
extern u8           D_8010D324[];
extern s32          D_8005ED70;
extern s32          D_8005ED74;
extern s32          D_8005ED78;

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
void Gp_ClearCollectedBit(s32 arg0);
void Gp_SetCollectedBit(s32 arg0);
s32  Gp_FindScanQty(GpItemRec* arg0, GpItemScan* arg1, s32* arg2, s32 arg3);
void Gp_DrawHpMpStats(UiPanel* arg0, s32 arg1);
void func_801061F0(void);
void Gp_NoticePanelTask(Task* arg0);

s32 func_800B7420(s32 arg0)
{
    GpItemScan scan;
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
                memset(&scan, 0, sizeof(scan));
                scan.field_1 = 0xFF;
                if (Gp_SumScanQty(&scan, id)) {
                    return 1;
                }
            }
            return 0;

        case 0x80:
        case 0x83:
            for (i = 0; i < 2; i++) {
                id = D_8010D320[i];
                memset(&scan, 0, sizeof(scan));
                scan.field_1 = 0xFF;
                if (Gp_SumScanQty(&scan, id)) {
                    return 1;
                }
            }
            return 0;

        case 0x9D:
        case 0x9E:
        case 0x9F:
            for (i = 0; i < 3; i++) {
                id = D_8010D324[i];
                memset(&scan, 0, sizeof(scan));
                scan.field_1 = 0xFF;
                if (Gp_SumScanQty(&scan, id)) {
                    return 1;
                }
            }
            return 0;

        case 0x9:
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x9F)) {
                return 1;
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x9E)) {
                memset(&scan, 0, sizeof(scan));
                scan.field_1 = 0xFF;
                if (Gp_SumScanQty(&scan, 0x9)) {
                    return 1;
                }
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            return Gp_SumScanQty(&scan, 0x9) >= 2;

        case 0xA:
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x94)) {
                return 1;
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x93)) {
                memset(&scan, 0, sizeof(scan));
                scan.field_1 = 0xFF;
                if (Gp_SumScanQty(&scan, 0xA)) {
                    return 1;
                }
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            return Gp_SumScanQty(&scan, 0xA) >= 2;

        case 0xC:
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x80)) {
                return 1;
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0xC)) {
                return 1;
            }
            /* Keeps GCC from cross-jumping the identical two-probe case tails. */
            SOFT_COMPILER_BARRIER();
            return 0;

        case 0x42:
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x98)) {
                return 1;
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x42)) {
                return 1;
            }
            SOFT_COMPILER_BARRIER();
            return 0;

        case 0x43:
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x9B)) {
                return 1;
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x43)) {
                return 1;
            }
            SOFT_COMPILER_BARRIER();
            return 0;

        case 0x44:
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x9C)) {
                return 1;
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x44)) {
                return 1;
            }
            SOFT_COMPILER_BARRIER();
            return 0;

        case 0x45:
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x9A)) {
                return 1;
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x45)) {
                return 1;
            }
            SOFT_COMPILER_BARRIER();
            return 0;

        case 0x46:
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x99)) {
                return 1;
            }
            memset(&scan, 0, sizeof(scan));
            scan.field_1 = 0xFF;
            if (Gp_SumScanQty(&scan, 0x46)) {
                return 1;
            }
            SOFT_COMPILER_BARRIER();
            return 0;

        default:
            if ((u32)(arg0 - 0x60) < 0x40) {
                memset(&scan, 0, sizeof(scan));
                scan.field_1 = 0xFF;
                return Gp_SumScanQty(&scan, arg0);
            }
            return 0;
    }
}

void Gp_RecalcMaxMp(void)
{
    WipSysConfig* cfg;
    McSaveData*   save;
    GpRec16*      table;
    GpStatRow*    rows;
    s8*           levels;
    s32           acc;
    s32           i;
    register s32  base asm("a3");
    s32           j;
    s32           count;
    register s32  start asm("a1");
    s32           limit;
    s32           idx;

    cfg    = &Wip_SysConfig;
    acc    = 0;
    levels = (s8*)Mc_SaveData.unknown_850;
    i      = acc;
    table  = Gp_IdParamHi;
    base   = acc;
    while (i < 0xC) {
        count = *levels;
        if (count > 0) {
            j = 0;
            if (j < count) {
                start = base;
                limit = count;
                do {
                    idx  = start + j;
                    idx  = idx + 1;
                    acc += table[idx].field[1];
                    j++;
                } while (j < limit);
            }
            USE_REG(start);
        }
        levels++;
        i++;
        base += 3;
    }
    if (cfg->field_23 != 0) {
        acc += Gp_ModStatAttrs[cfg->field_23 - 1].field_6;
    }
    rows          = Gp_StatRows;
    save          = &Mc_SaveData;
    acc          += rows[save->field_F].field_4;
    acc          += save->field_27;
    cfg->field_1e = acc;
    if ((s16)acc >= 0xFB) {
        cfg->field_1e = 0xFA;
    }
    if (cfg->field_1c > cfg->field_1e) {
        cfg->field_1c = cfg->field_1e;
    }
}

void Gp_EquipMod(s32 arg0)
{
    WipSysConfig* cfg;
    GpItemRec*    rec;
    GpItemRec*    tmp;
    GpItemScan*   scan;
    s32           i;

    cfg = &Wip_SysConfig;
    if ((u32)(arg0 - 0x60) < 0x20U) {
        if (cfg->field_23 != (arg0 - 0x5F)) {
            GpItemRec* found;
            s32        neg;

            found = Gp_FindItemById(arg0);
            if (found != NULL) {
                neg            = -1;
                found->field_1 = neg;
                if (cfg->field_23 != 0) {
                    found = Gp_FindItemById(cfg->field_23 + 0x5F);
                    if (found != NULL) {
                        found->field_1 = 0;
                    }
                }
                cfg->field_23 = arg0 - 0x5F;

                {
                    WipSysConfig* p;
                    McSaveData*   save;
                    GpStatRow*    table;
                    u16           val;

                    p           = &Wip_SysConfig;
                    table       = Gp_StatRows;
                    save        = &Mc_SaveData;
                    val         = table[save->field_F].field_0;
                    p->field_1a = val;
                    val        += save->field_26;
                    p->field_1a = val;
                    if (p->field_23 != 0) {
                        val        += Gp_ModStatAttrs[p->field_23 - 1].field_4;
                        p->field_1a = val;
                    }
                    if (p->field_1a >= 0xFB) {
                        p->field_1a = 0xFA;
                    }
                    if (p->field_18 > p->field_1a) {
                        p->field_18 = p->field_1a;
                    }

                    scan = &save->field_5BC;
                    Gp_RecalcMaxMp();
                    switch (scan->field_2) {
                        case 2:
                            tmp = Gp_ItemTable2;
                            break;
                        case 1:
                            tmp = Gp_ItemTable1;
                            break;
                        default:
                            tmp = save->field_1AC;
                            break;
                    }
                }
                i   = 0;
                rec = &tmp[scan->field_0];
                if (scan->field_1 != 0) {
                    do {
                        Gp_RefreshItemRow(rec);
                        i++;
                        rec++;
                    } while (i < scan->field_1);
                }

                {
                    McSaveData* p;
                    s32         word;
                    s32         bit;

                    word = arg0 / 32;
                    bit  = 1 << (arg0 % 32);
                    if ((u32)arg0 < 0x180U) {
                        p                   = &Mc_SaveData;
                        p->field_6D0[word] |= bit;
                    }
                }
            }
        }
    } else if (arg0 == 0) {
        McSaveData* save;
        GpStatRow*  table;
        u16         val;

        table         = Gp_StatRows;
        save          = &Mc_SaveData;
        val           = table[save->field_F].field_0;
        cfg->field_1a = val;
        val          += save->field_26;
        cfg->field_1a = val;
        if (cfg->field_23 != 0) {
            val          += Gp_ModStatAttrs[cfg->field_23 - 1].field_4;
            cfg->field_1a = val;
        }
        if (cfg->field_1a >= 0xFB) {
            cfg->field_1a = 0xFA;
        }
        if (cfg->field_18 > cfg->field_1a) {
            cfg->field_18 = cfg->field_1a;
        }
        Gp_RecalcMaxMp();
    } else {
        return;
    }
    Gp_HpMpWork.field_0 = cfg->field_18;
    Gp_HpMpWork.field_4 = cfg->field_1c;
}

void Gp_InitStarterInv(void)
{
    GpItemScan*   scan;
    McSaveData*   save;
    WipSysConfig* cfg;
    WipSysConfig* cfg2;
    GpItemRec*    tmp;
    GpItemRec*    rec;
    GpItemRec*    added;
    GpItemScan**  scans;
    GpItemScan*   dest;
    GpItemSlot*   slots;
    s32           i;
    s32           j;
    u8            item;
    s32           three;
    u16           hp;
    u16           mp;
    s32           flag105;
    s32           flag107;

    scan               = &Mc_SaveData.field_5BC;
    save               = &Mc_SaveData;
    save->field_908[5] = 0;
    save->field_908[0] = 0;
    cfg                = &Wip_SysConfig;
    switch (scan->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    rec  = &tmp[scan->field_0];
    dest = D_8010D55C;
    i    = 0;
    if (scan->field_1 != 0) {
        do {
            item = rec->field_0;
            if (item != 0) {
                if ((u8)(item + 0x63) < 3) {
                    Gp_GiveItem(dest, 0x3D, 1);
                } else if (item == 0x8A) {
                    Gp_GiveItem(dest, 0x3C, 1);
                } else if (item == 0x65) {
                    Gp_GiveItem(dest, 0xD, 1);
                } else if ((item != 0x81) && (item != 0xA0) && (item != 0x60) &&
                           (item != 0x40) && (item != 0x92)) {
                    Gp_GiveItem(dest, rec->field_0, rec->field_2);
                }
            }
            i++;
            rec++;
        } while (i < scan->field_1);
    }
    Gp_ClearScanItems(scan);
    scans = Gp_ScanPtrs;
    Gp_ClearScanItems(scans[1]);
    Gp_ClearScanItems(scans[2]);
    slots = Mc_SaveData.field_5C8;
    for (j = 0; j < 0x20; j++) {
        slots->field_0 = 0;
        slots->field_1 = 0;
        slots->field_2 = 0xFF;
        slots->field_3 = 0;
        if (j == 0x1A) {
            slots->field_2 = 0;
            slots->field_3 = 0;
        }
        slots->field_4 = 0;
        slots++;
    }
    three = 3;
    Gp_ApplyItemMap();
    Gp_GiveItem(scan, 0x63, 1);
    Game_Session->field_11C = -1;
    cfg->field_21           = 0;
    cfg->field_26           = three;
    Gp_EquipMod(0x63);
    added          = Gp_GiveItem(scan, 0x40, 1);
    added->field_1 = 1;
    added          = Gp_GiveItem(scan, 2, 1);
    added->field_1 = 2;
    added          = Gp_GiveItem(scan, 0x81, 1);
    added->field_1 = three;
    Gp_GiveItem(scan, 0xA0, 0x64);
    Gp_EquipRelatedItem(scan, 0x81, 0xA0, -1);
    Gp_GiveItem(scan, 0x92, 1);
    cfg2           = &Wip_SysConfig;
    hp             = cfg2->field_1a;
    mp             = cfg2->field_1e;
    cfg2->field_18 = hp;
    cfg2->field_1c = mp;
    flag105        = Gp_HasCollectedBit(0x105);
    flag107        = Gp_HasCollectedBit(0x107);
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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B8014);

void Gp_MoveItemSlot(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    register GpItemRec* tmp asm("v0");
    register GpItemRec* table asm("a3");
    GpItemRec*          rec;
    GpItemRec*          src;
    GpItemRec           saved;
    s32                 i;
    register s32        off asm("v0");

    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;

    if (arg1 == arg2) {
        return;
    }

    arg1 += arg0->field_0;
    arg2 += arg0->field_0;
    i     = arg2;

    if (arg1 < arg2) {
        off          = arg2 << 2;
        rec          = (GpItemRec*)(off + (s32)table);
        off          = arg1 << 2;
        src          = (GpItemRec*)(off + (s32)table);
        saved        = *src;
        src->field_0 = 0;
        src->field_2 = 0;
    loop1:
        if (rec->field_0 != 0) {
            i--;
            if (arg1 < i) {
                rec--;
                goto loop1;
            }
        }
        if (i < arg2) {
            off = i << 2;
            rec = (GpItemRec*)(off + (s32)table);
            do {
                *rec = rec[1];
                i++;
                rec++;
            } while (i < arg2);
        }
    } else {
        off          = arg1 << 2;
        src          = (GpItemRec*)(off + (s32)table);
        saved        = *src;
        src->field_0 = 0;
        src->field_2 = 0;
        if (arg2 < arg1) {
            off = arg2 << 2;
            rec = (GpItemRec*)(off + (s32)table);
        loop2:
            if (rec->field_0 != 0) {
                i++;
                if (i < arg1) {
                    rec++;
                    goto loop2;
                }
            }
        }
        if (arg2 < i) {
            off = i << 2;
            rec = (GpItemRec*)(off + (s32)table);
            do {
                *rec = rec[-1];
                i--;
                rec--;
            } while (arg2 < i);
        }
    }
    *(GpItemRec*)((arg2 << 2) + (s32)table) = saved;
}

void Gp_SortItems(GpItemScan* arg0, s32 arg1)
{
    GpItemRec*          tmp;
    register GpItemRec* table;
    GpItemRec*          rec;
    GpItemRec*          other;
    GpItemRec           saved;
    s32                 i;
    s32                 j;
    s32                 key;
    register s32        minKey asm("t2");
    s32                 id;
    s32                 idx;
    s32                 next;
    s32                 count;
    register s32        dummy5 asm("s5");
    register s32        dummy6 asm("s6");
    register s32        dummy7 asm("s7");

    i = 0;
    if ((arg0->field_1 - 1) > 0) {
        do {
            switch (arg0->field_2) {
                case 2:
                    tmp = Gp_ItemTable2;
                    break;
                case 1:
                    tmp = Gp_ItemTable1;
                    break;
                default:
                    tmp = Mc_SaveData.field_1AC;
                    break;
            }
            table = tmp;
            rec   = (GpItemRec*)((s32)table + (arg0->field_0 << 2));
            rec   = (GpItemRec*)((s32)rec + (i << 2));
            id    = rec->field_0;

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

            if (arg0->field_2 != 1) {
                tmp = Mc_SaveData.field_1AC;
                if (arg0->field_2 == 2) {
                    tmp = Gp_ItemTable2;
                }
            } else {
                tmp = Gp_ItemTable1;
            }
            table = tmp;
            j     = i + 1;
            other = (GpItemRec*)((s32)table + (arg0->field_0 << 2));
            next  = (i << 2) + 4;
            other = (GpItemRec*)((s32)other + next);
            if (j < arg0->field_1) {
                do {
                    id = other->field_0;

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
                    count = arg0->field_1;
                    j++;
                    other++;
                } while (j < count);
            }
            count = arg0->field_1;
            i++;
        } while (i < (count - 1));
    }
}

s32 Gp_CanAddItemQty(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("t1");
    GpItemRec*          rec;
    register s32        i asm("t0");
    s32                 occupied;
    s32                 count;
    s32                 start;
    s32                 limit;
    s32                 used;
    s32                 found;
    GpItemRec*          table2;
    GpItemRec*          walker;
    s32                 count2;
    s32                 start2;
    GpItemA0*           p;
    s32                 idx;
    GpItemA0*           cap;
    s32                 capacity;

    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table    = tmp;
    i        = 0;
    count    = arg0->field_1;
    start    = arg0->field_0;
    occupied = i;
    if (count != 0) {
        limit = count;
        rec   = (GpItemRec*)((start << 2) + (s32)table);
        do {
            if (rec->field_0 != 0) {
                occupied++;
            }
            i++;
            rec++;
        } while (i < limit);
    }

    capacity = arg0->field_1;
    used     = occupied;
    if (arg1 >= 0x100) {
        return 1;
    }

    if (arg1 >= 0xA0) {
        found  = 0;
        start2 = arg0->field_0;
        switch (arg0->field_2) {
            case 2:
                table2 = Gp_ItemTable2;
                break;
            case 1:
                table2 = Gp_ItemTable1;
                break;
            default:
                table2 = Mc_SaveData.field_1AC;
                break;
        }
        if (arg2 < 0) {
            arg2 = Gp_StackLimits[arg1 - 0xA0].field_0;
        }
        i      = 0;
        count2 = arg0->field_1;
        if (count2 != 0) {
            p      = Gp_StackLimits;
            idx    = arg1 - 0xA0;
            cap    = (GpItemA0*)((idx << 2) + (s32)p);
            walker = (GpItemRec*)((start2 << 2) + (s32)table2);
            do {
                if (walker->field_0 == arg1) {
                    found = 2;
                    if (cap->field_2 >= walker->field_2 + arg2) {
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

s32 Gp_CanAddItem(GpItemScan* arg0, s32 arg1)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("t0");
    GpItemRec*          rec;
    register s32        i asm("a3");
    s32                 occupied;
    s32                 count;
    s32                 start;
    s32                 limit;
    s32                 used;
    s32                 found;
    GpItemRec*          table2;
    GpItemRec*          walker;
    s32                 count2;
    s32                 start2;
    GpItemA0*           p;
    s32                 idx;
    GpItemA0*           cap;
    s32                 capacity;

    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table    = tmp;
    i        = 0;
    count    = arg0->field_1;
    start    = arg0->field_0;
    occupied = i;
    if (count != 0) {
        limit = count;
        rec   = (GpItemRec*)((start << 2) + (s32)table);
        do {
            if (rec->field_0 != 0) {
                occupied++;
            }
            i++;
            rec++;
        } while (i < limit);
    }

    capacity = arg0->field_1;
    used     = occupied;
    if (arg1 >= 0x100) {
        return 1;
    }

    if (arg1 >= 0xA0) {
        found  = 0;
        start2 = arg0->field_0;
        switch (arg0->field_2) {
            case 2:
                table2 = Gp_ItemTable2;
                break;
            case 1:
                table2 = Gp_ItemTable1;
                break;
            default:
                table2 = Mc_SaveData.field_1AC;
                break;
        }
        i      = 0;
        count2 = arg0->field_1;
        if (count2 != 0) {
            p      = Gp_StackLimits;
            idx    = arg1 - 0xA0;
            cap    = (GpItemA0*)((idx << 2) + (s32)p);
            walker = (GpItemRec*)((start2 << 2) + (s32)table2);
            do {
                if (walker->field_0 == arg1) {
                    if (walker->field_2 < cap->field_2) {
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

GpItemRec* Gp_SetScanItem(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("s1");
    GpItemRec*          dest;
    GpItemRec*          walker;
    register GpItemRec* rec asm("v1");
    GpItemRec*          found;
    GpItemRec*          slot;
    s32                 i;
    s32                 start;
    register s32        idx asm("v1");
    register s32        field0 asm("v0");
    s32                 item;
    s32                 qty;

    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    if ((u32)(arg2 - 0xA0) < 0x20U) {
        dest = Gp_GiveItem(arg0, arg2, arg3);
        i    = arg0->field_0;
        if (((GpItemRec*)(((i + arg1) << 2) + (s32)table))->field_0 != 0) {
            goto done;
        }
        start = i;
        USE_REG(start);
        if (arg0->field_1 == 0) {
            return dest;
        }
        i      = 0;
        walker = (GpItemRec*)((start << 2) + (s32)table);
        do {
            if (walker->field_0 == arg2) {
                if (i == arg1) {
                    return dest;
                }
                dest           = (GpItemRec*)((s32)table + ((arg0->field_0 + arg1) << 2));
                dest->field_0  = arg2;
                found          = walker;
                slot           = (GpItemRec*)(((arg0->field_0 + arg1) << 2) + (s32)table);
                slot->field_2  = found->field_2;
                found->field_0 = 0;
                table          = found;
                table->field_2 = 0;
                goto done;
            }
            i++;
            walker++;
        } while (i < arg0->field_1);
        return dest;
    }
    field0 = arg0->field_0;
    idx    = field0 + arg1;
    field0 = idx << 2;
    USE_REG(field0);
    rec = (GpItemRec*)(field0 + (s32)tmp);
    if (rec->field_0 == 0) {
        dest          = rec;
        dest->field_0 = arg2;
        dest->field_2 = 1;
        goto done;
    }
    dest          = rec;
    item          = dest->field_0;
    qty           = dest->field_2;
    dest->field_0 = arg2;
    dest->field_2 = 1;
    Gp_GiveItem(arg0, item, qty);
done:
    return dest;
}

GpItemRec* Gp_AddItem(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    register GpItemRec* tmp asm("v0");
    register GpItemRec* table asm("t4");
    register GpItemRec* dest asm("t2");
    register s32        found asm("t3");
    register s32        start asm("t1");
    register s32        i asm("t0");
    register s32        idx asm("v1");
    register s32        off asm("v0");
    register GpItemRec* result asm("v0");
    GpItemRec*          rec;
    GpItemA0*           attrs;
    s32                 temp;
    s32                 count;

    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    dest  = NULL;
    table = tmp;
    if (arg2 < 0) {
        temp = arg1 - 0xA0;
        if ((u32)temp < 0x20U) {
            if (arg2 == -2) {
                arg2 = Gp_StackLimits[temp].field_2;
            } else {
                arg2 = Gp_StackLimits[temp].field_0;
            }
        } else {
            arg2 = 1;
        }
    }

    start = arg0->field_0;
    idx   = arg1 - 0xA0;
    found = 0;
    if ((u32)idx < 0x20U) {
        i = found;
        if (arg0->field_1 != 0) {
            attrs = Gp_StackLimits;
            idx   = (idx << 2) + (s32)attrs;
            do {
                rec = (GpItemRec*)((start << 2) + (s32)table);
                if (rec->field_0 == arg1) {
                    arg2 += rec->field_2;
                    if (((GpItemA0*)idx)->field_2 < arg2) {
                        arg2 = ((GpItemA0*)idx)->field_2;
                    }
                    rec->field_2 = arg2;
                    dest         = rec;
                    found        = 1;
                    break;
                }
                i++;
                start++;
            } while (i < arg0->field_1);
        }
        result = dest;
        if (found != 0) {
            goto ret;
        }
        start = arg0->field_0;
        if (arg0->field_1 == 0) {
            goto done;
        }
        SOFT_TOUCH_REG(found);
        i     = 0;
        attrs = Gp_StackLimits;
        idx   = arg1 - 0xA0;
        idx   = (idx << 2) + (s32)attrs;
        off   = start << 2;
        rec   = (GpItemRec*)(off + (s32)table);
        do {
            if (rec->field_0 == 0) {
                rec->field_0 = arg1;
                if (((GpItemA0*)idx)->field_2 < arg2) {
                    arg2 = ((GpItemA0*)idx)->field_2;
                }
                dest          = rec;
                dest->field_2 = arg2;
                dest->field_1 = 0;
                goto done;
            }
            i++;
            rec++;
        } while (i < arg0->field_1);
        return dest;
    }

    goto loop_header;
fill:
    dest          = (GpItemRec*)idx;
    dest->field_0 = arg1;
    dest->field_2 = 1;
    dest->field_1 = 0;
    goto done;
loop_header:
    count = arg0->field_1;
    i     = 0;
    if (count != 0) {
        off = start << 2;
        idx = off + (s32)table;
        do {
            if (((GpItemRec*)idx)->field_0 == 0) {
                goto fill;
            }
            i++;
            idx += 4;
        } while (i < count);
    }
done:
    result = dest;
ret:
    return result;
}

char* Gp_GetItemText(s32 arg0, s32 arg1, s32 arg2)
{
    s8*          str;
    GpItemDesc*  table;
    GpItemDesc*  desc;
    s32          bit;
    s32          val;
    s8*          prev;
    register s32 tmp asm("v1");

    if (arg0 >= 0x500) {
        str = Gp_ItemTextHi[arg0 - 0x500];
    } else if (arg0 >= 0x300) {
        tmp  = arg0 & 3;
        arg2 = (arg0 & 0xF0) >> 4;
        arg0 = (arg0 & 0xC) >> 2;
        if (tmp == 0) {
            tmp = 1;
        }
        arg0 = (arg2 * 3 + arg0) * 3;
        val  = tmp + 0xE;
        str  = Gp_GetItemText(arg0 + val, arg1, 1);
    } else {
        tmp = arg0 << 3;
        if (arg0 < 0x100) {
            table = Gp_ItemDescs;
        } else {
            table = Gp_ItemDescsHi;
        }
        desc = (GpItemDesc*)(tmp + (s32)table);
        if (arg2 == 0) {
            arg2 = arg0 / 32;
            bit  = 1 << (arg0 % 32);
            if ((u32)arg0 >= 0x180) {
                arg2 = 1;
            } else {
                val  = Mc_SaveData.field_6D0[arg2] & bit;
                arg2 = val != 0;
            }
        }
        str = desc->field_4;
        if (arg1 >= 3) {
            arg1 = 0;
        }
        if (arg2 == 0) {
            arg1 += 3;
        }
        if (arg1 > 0) {
            s32 c_nl = 0xA;
            s32 c_n  = 0x6E;
            s32 c_bs = 0x5C;
            s32 c_N  = 0x4E;
            prev     = str - 1;
        loop:
            arg0 = *str;
            if (arg0 == 0 || arg0 == c_nl) {
                arg1--;
            } else if (arg0 == c_n && *prev == c_bs) {
                arg1--;
            } else if (arg0 == c_N && *prev == c_bs) {
                arg1--;
            }
            prev++;
            str++;
            if (arg1 > 0) {
                goto loop;
            }
        }
    }
    return str;
}

s32 Gp_NthRelatedId(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    GpItemRec*          tmp;
    GpItemRec*          table;
    register GpItemRec* rec asm("t1");
    GpItemRec*          rec2;
    WipSysConfig*       cfg;
    GpItemQty*          table0;
    GpItemQty*          table1;
    s32                 idx;
    register s32        i asm("a3");
    s32                 item;
    s32                 off;
    s32                 temp;

    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    idx   = arg0->field_0;
    cfg   = &Wip_SysConfig;
    if (arg1 >= 0) {
        table0 = Gp_RelatedQty0;
        table1 = Gp_RelatedQty1;
        temp   = idx << 2;
        rec    = (GpItemRec*)(temp + (s32)table);
        do {
            if ((u8)(rec->field_0 + 0x80) < 0x20) {
                rec2 = rec;
                if (arg2 == 0) {
                    goto decrement;
                }
                i = 0;
                USE_REG(i);
                item = rec->field_0;
                off  = (item - 0x80) * 4;
                item = item - 0x7F;
                do {
                    temp = i + off;
                    if (((GpItemQty*)(temp + (s32)table0))->field_1 == arg2) {
                        if ((s8)rec2->field_1 > 0) {
                            arg1--;
                        } else if (cfg->field_21 == item) {
                            arg1--;
                        }
                        break;
                    }
                    i++;
                } while (i < 3);

                i    = 0;
                item = rec->field_0;
                rec2 = rec;
                off  = (item - 0x80) * 4;
                item = item - 0x7F;
                do {
                    temp = i + off;
                    if (((GpItemQty*)(temp + (s32)table1))->field_1 == arg2) {
                        if ((s8)rec2->field_1 > 0) {
                            goto decrement;
                        }
                        if (cfg->field_21 != item) {
                            goto next;
                        }
                    decrement:
                        arg1--;
                        goto next;
                    }
                    i++;
                } while (i < 3);
            }
        next:
            rec++;
            idx++;
        } while (arg1 >= 0);
    }
    idx--;
    return table[idx].field_0;
}

void Gp_RefreshItemRow(GpItemRec* arg0)
{
    u8           item;
    GpItemSlot*  slot;
    register s32 found asm("a3");
    s32          i;
    GpItemMap*   p;
    s32          inRange;

    if ((s8)arg0->field_1 <= 0) {
        return;
    }

    inRange       = (u8)(arg0->field_0 + 0x80) < 0x20;
    arg0->field_1 = 0;
    if (!inRange) {
        return;
    }

    item = arg0->field_0;
    if (item == Wip_SysConfig.field_21 + 0x7F) {
        return;
    }

    if ((u32)(item - 0x80) >= 0x20) {
        return;
    }

    found = 0;
    slot  = &Mc_SaveData.field_1C8[item];
    for (i = found, p = Gp_ItemMaps; i < 8; i++, p++) {
        if (item == p->field_1) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
        slot->field_0 = 0;
        slot->field_1 = 0;
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
        if (slot->field_2 != 0xFF) {
            slot->field_2 = 0;
        }
        slot->field_3 = 0;
    }
}

void func_800B92CC(void)
{
    switch (*(u32*)&Mc_SaveData.field_4 & 0xFFFF0000) {
        case 0x1010000:
            func_80180804();
            break;
        case 0x10F0000:
            func_8017EA68();
            break;
        case 0x1130000:
            func_80181468();
            break;
        case 0x2010000:
            func_8017EA90();
            break;
        case 0x2110000:
            func_8017E9E8();
            break;
        case 0x21B0000:
            func_80181364();
            break;
        case 0x3010000:
            func_8017E9F8();
            break;
        case 0x3110000:
            func_8017EAE0();
            break;
        case 0x31B0000:
            func_8018138C();
            break;
        case 0x4060000:
            func_8017EA78();
            break;
        case 0x4100000:
            func_8017EB2C();
            break;
        case 0x4140000:
            func_8017EDE8();
            break;
        case 0x41F0000:
            func_8017EAB4();
            break;
        case 0x4290000:
            func_8017EA64();
            break;
        case 0x42F0000:
            func_8017EC04();
            break;
        case 0x5160000:
            func_8017EAC4();
            break;
        case 0x51C0000:
            func_8017EA60();
            break;
        case 0x21E0000:
            func_8017EA58();
            break;
        case 0x31E0000:
            func_8017EA74();
            break;
    }
}

static __inline void func_800B996C_RemoveItem(GpItemScan* arg0, GpItemRec* arg1, s32 arg2)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("v1");
    register s32        qty asm("a2");
    register GpItemRec* base asm("t0");
    register s32        item asm("t1");
    GpItemRec*          rec;
    s32                 i;
    s32                 count;
    register s32        end asm("v1");
    register s32        loop_end asm("a1");
    register s32        newQty asm("v1");

    item = arg1->field_0;
    if (item < 0xA0) {
        arg1->field_0 = 0;
        arg1->field_2 = 0;
        arg1->field_1 = 0;
    } else {
        switch (arg0->field_2) {
            case 2:
                tmp = Gp_ItemTable2;
                break;
            case 1:
                tmp = Gp_ItemTable1;
                break;
            found:
                qty = rec->field_2;
                goto after_loop;
            default:
                tmp = Mc_SaveData.field_1AC;
                break;
        }
        table = tmp;
        qty   = 0;
        USE_REG(qty);
        i     = arg0->field_0;
        count = arg0->field_1;
        base  = table;
        end   = i + count;
        if (i < end) {
            loop_end = end;
            rec      = (GpItemRec*)((i << 2) + (s32)base);
        loop:
            if (rec->field_0 != item) {
                i++;
                rec++;
                if (i < loop_end) {
                    goto loop;
                }
            } else {
                goto found;
            }
        }
    after_loop:
        if (i != arg0->field_0 + arg0->field_1) {
            if (arg2 < 0) {
                arg2 = qty;
            }
            newQty = qty - arg2;
            if (newQty < 0) {
                newQty = 0;
            }
            if (newQty == 0) {
                base[i].field_0 = 0;
                base[i].field_2 = 0;
                base[i].field_1 = 0;
            } else {
                base[i].field_2 = newQty;
            }
        }
    }
}

void Gp_UiBoostAttach(UiObject* arg0, Task* arg1)
{
    register UiObject* obj asm("s7");
    register Task*     task asm("s6");
    register s32       sel asm("a0");
    register s32       color asm("s5");
    register s32       y0 asm("s4");
    register s32       one asm("s3");
    register s32       x asm("s2");
    register s32       y asm("s1");
    s32                item;
    s32                level;
    s32                width;
    s32                other;
    s32                saved;
    u8*                str;
    char*              text;
    GpItemAttr*        p;

    obj  = arg0;
    sel  = Wip_SysConfig.field_23;
    task = arg1;
    item = sel + 0x5F;
    if (task->state == 0) {
        task->flags = 0xFF;
        sel         = sel - 1;
        level       = 0;
        if ((u32)sel < 0x20U) {
            p      = &Gp_ItemAttrs[item];
            level  = p->field_5;
            level += Mc_SaveData.field_908[sel];
            USE_REG(sel);
            if (level >= 0xB) {
                level = 0xA;
            }
        }
        if (level < 0xA) {
            register s32 idx asm("v1");
            McSaveData*  save;
            save = &Mc_SaveData;
            TOUCH_REG(save);
            idx                         = item - 0x60;
            *(u8*)&save->field_908[idx] = *(u8*)&save->field_908[idx] + 1;
        } else {
            task->flags = 0x1A;
        }
        if (task->flags != 0xFF) {
            goto error;
        }
        width = Text_MeasureWidth(Gp_GetItemText(item, 0, 0)) + Text_MeasureWidth(Gp_StrMore) + 4;
        other = Text_MeasureWidth(Gp_StrAttachAvail);
        if (width < other) {
            width = other;
        }
        Ui_UpdateLayoutSize((UiPanel*)obj, width + 5, Ui_Scale15(2) + 1);
        {
            register s32 tx asm("v1");
            register s32 ty asm("v0");
            tx = (-((UiPanel*)obj)->field_C.w) >> 1;
            ty = ((-((UiPanel*)obj)->field_C.h) >> 1) - 0x14;
            USE_REG(obj);
            ((UiPanel*)obj)->field_C.y = ty;
            ((UiPanel*)obj)->field_C.x = tx;
        }
        func_800B996C_RemoveItem(&Mc_SaveData.field_5BC, Gp_SelItemRec, 1);
        task->killCountdown = 0xBC;
        task->state         = task->state + 1;
    }
    if (task->flags != 0xFF) {
    error:
        saved           = task->spawnArg1;
        task->spawnArg1 = task->flags;
        Gp_NoticePanelTask(task);
        task->spawnArg1 = saved;
        return;
    }

    {
        register UiObject* a0obj asm("a0");
        register char*     notice asm("a1");

        a0obj = obj;
        TOUCH_REG(a0obj);
        notice = Gp_StrNotice2;
        TOUCH_REG(notice);
        color = 0x606060;
        TOUCH_REG(color);
        x  = obj->field_1C;
        y0 = (s16)obj->field_18;
        Ui_DrawText((UiPanel*)a0obj, notice);
        x = x + 2;
        TOUCH_REG(x);
    }
    y   = y0 + 0xF;
    str = Gp_StrMore;
    one = 1;
    Text_DrawPrompt(obj, x, y, str, color, one, 0);
    {
        register s32 w asm("v0");
        register s32 a0item asm("a0");
        register s32 za asm("a1");
        register s32 zb asm("a2");
        w      = Text_MeasureWidth(str);
        a0item = item;
        za     = 0;
        zb     = za;
        USE_REG5(str, za, zb, w, a0item);
        width = w + 4;
        text  = Gp_GetItemText(a0item, za, zb);
    }
    Text_DrawPrompt(obj, x + width, y, text, 0x37A78, one, 0);
    Text_DrawPrompt(obj, x, y0 + 0x1E, Gp_StrAttachAvail, color, one, 0);

    if (obj->status == one) {
        task->killCountdown = task->killCountdown - 1;
        if ((task->killCountdown <= 0) || (Pad_CheckButtons(0, one, D_8005ED70 | D_8005ED74) != 0)) {
            obj->field_2E       = 9;
            task->killCountdown = 0x7FFF;
        } else if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            obj->field_2E       = -1;
            task->killCountdown = 0x7FFF;
        }
    }
}

void Gp_UiBoostMp(UiObject* arg0, Task* arg1)
{
    WipSysConfig* cfg;
    McSaveData*   save;
    s32           saved;

    if (arg1->state == 0) {
        cfg                 = &Wip_SysConfig;
        Gp_HpMpWork.field_0 = cfg->field_18;
        save                = &Mc_SaveData;
        Gp_HpMpWork.field_4 = cfg->field_1c;
        if (save->field_27 < 0xFA) {
            save->field_27 = save->field_27 + 1;
        }
        Gp_RecalcMaxMp();
        cfg->field_1c = cfg->field_1e;
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
    WipSysConfig* cfg;
    McSaveData*   save;
    s32           saved;
    s32           hp;
    u16           val;

    if (arg1->state == 0) {
        cfg                 = &Wip_SysConfig;
        hp                  = cfg->field_18;
        Gp_HpMpWork.field_0 = hp;
        save                = &Mc_SaveData;
        Gp_HpMpWork.field_4 = cfg->field_1c;
        if (save->field_26 < 0xFA) {
            save->field_26 = save->field_26 + 5;
        }
        val           = Gp_StatRows[save->field_F].field_0;
        cfg->field_1a = val;
        val          += save->field_26;
        cfg->field_1a = val;
        if (cfg->field_23 != 0) {
            val          += Gp_ModStatAttrs[cfg->field_23 - 1].field_4;
            cfg->field_1a = val;
        }
        if (cfg->field_1a >= 0xFB) {
            cfg->field_1a = 0xFA;
        }
        if (cfg->field_1a < hp) {
            cfg->field_18 = cfg->field_1a;
        }
        cfg->field_18 = cfg->field_1a;
        func_800B996C_RemoveItem(0, Gp_SelItemRec, 1);
        Ui_SpawnFromDesc(&Gp_BoostPanelDesc, 0, 0, 1, arg0);
    }
    saved           = arg1->spawnArg1;
    arg1->spawnArg1 = 0x1C;
    Gp_NoticePanelTask(arg1);
    arg1->spawnArg1 = saved;
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B9D80);

void Gp_ResetInventory(void)
{
    WipSysConfig*        cfg;
    register s32         item asm("a1");
    GpItemSlot*          slot;
    s32                  found;
    register s32         i asm("a0");
    GpItemMap*           p;
    GpItemScan*          scan;
    GpItemRec*           tmp;
    register GpItemRec*  table asm("v1");
    s32                  count;
    s32                  start;
    s32                  off;
    register McSaveData* save asm("v0");
    GpItemScan*          dest;
    register GpItemScan* src asm("t4");
    WipSysConfig*        pcfg;
    u16                  hp;
    u16                  mp;
    u8*                  levels;
    s32                  col;
    GpStateC08*          state;
    s32                  val;

    cfg = &Wip_SysConfig;
    val = cfg->field_21;
    if (val != 0) {
        SCHED_BARRIER();
        asm("addiu %0, %1, 0x7F" : "=r"(item) : "r"(val));
        if ((u32)(val - 1) < 0x20U) {
            found = 0;
            slot  = &Mc_SaveData.field_1C8[item];
            for (i = found, p = Gp_ItemMaps; i < 8; i++, p++) {
                if (item == p->field_1) {
                    found = 1;
                    break;
                }
            }
            if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
                slot->field_0 = 0;
                slot->field_1 = 0;
            }
            if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
                if (slot->field_2 != 0xFF) {
                    slot->field_2 = 0;
                }
                slot->field_3 = 0;
            }
            USE_REG(i);
        }
        cfg->field_21 = 0;
    }

    scan = &Gp_DefaultScan;
    switch (scan->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    i     = 0;
    count = scan->field_1;
    start = scan->field_0;
    if (count != 0) {
        off   = start << 2;
        table = (GpItemRec*)(off + (s32)table);
        do {
            i++;
            table->field_0 = 0;
            table->field_1 = 0;
            table->field_2 = 0;
            table++;
        } while (i < scan->field_1);
    }

    save = &Mc_SaveData;
    dest = &save->field_5BC;
    asm volatile("lui %0, %%hi(Gp_DefaultScan)" : "=r"(table));
    item = 0x6C;
    USE_REG(item);
    asm volatile("addiu %0, %1, %%lo(Gp_DefaultScan)" : "=r"(src) : "r"(table));
    USE_REG(table);
    *dest = *src;
    Gp_AddItem(dest, item, 1);
    Gp_EquipMod(0x6C);

    pcfg           = &Wip_SysConfig;
    hp             = pcfg->field_1a;
    mp             = pcfg->field_1e;
    pcfg->field_18 = hp;
    pcfg->field_1c = mp;
    Gp_ApplyItemMap();

    item   = 0;
    levels = Gp_DebugAttachLevels;
    i      = item;
    for (; item < 4; item++, i += 3) {
        for (col = 0; col < 3; col++) {
            *(u8*)((col + i) + (s32)levels) = 0;
        }
    }
    Gp_DebugAttachLevels[0] = 1;

    state          = &Gp_StateC08;
    state->field_5 = 0;
    state->field_B = 0;
}

void Gp_ClearInventory(void)
{
    WipSysConfig*       cfg;
    register s32        item asm("a1");
    GpItemSlot*         slot;
    s32                 found;
    register s32        i asm("a0");
    GpItemMap*          p;
    GpItemScan*         scan;
    GpItemRec*          tmp;
    register GpItemRec* table asm("v1");
    s32                 count;
    s32                 start;
    s32                 off;
    McSaveData*         save;
    WipSysConfig*       pcfg;
    u16                 hp;
    u16                 mp;
    GpStateC08*         state;
    s32                 val;
    GpItemScan*         dest;
    GpItemRec*          rec;
    s32                 j;
    GpStatRow*          rows;
    GpItemAttr*         attrs;
    McSaveData*         save2;
    u8                  id;
    WipSysConfig*       hpCfg;
    u16                 hpVal;
    s32                 idx;
    s32                 n;

    cfg = &Wip_SysConfig;
    USE_REG(cfg);
    val = cfg->field_21;
    if (val != 0) {
        SCHED_BARRIER();
        asm("addiu %0, %1, 0x7F" : "=r"(item) : "r"(val));
        if ((u32)(val - 1) < 0x20U) {
            found = 0;
            slot  = &Mc_SaveData.field_1C8[item];
            for (i = found, p = Gp_ItemMaps; i < 8; i++, p++) {
                if (item == p->field_1) {
                    found = 1;
                    break;
                }
            }
            if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
                slot->field_0 = 0;
                slot->field_1 = 0;
            }
            if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
                if (slot->field_2 != 0xFF) {
                    slot->field_2 = 0;
                }
                slot->field_3 = 0;
            }
            USE_REG(i);
        }
        cfg->field_21 = 0;
    }

    scan = &Gp_DefaultScan;
    switch (scan->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    i     = 0;
    count = scan->field_1;
    start = scan->field_0;
    if (count != 0) {
        off   = start << 2;
        table = (GpItemRec*)(off + (s32)table);
        do {
            i++;
            table->field_0 = 0;
            table->field_1 = 0;
            table->field_2 = 0;
            table++;
        } while (i < scan->field_1);
    }

    save                    = &Mc_SaveData;
    n                       = 0x14;
    dest                    = &save->field_5BC;
    save->field_5BC.field_0 = 0;
    save->field_5BC.field_1 = n;
    save->field_5BC.field_2 = 0;
    switch (dest->field_2) {
        case 2:
            rec = Gp_ItemTable2;
            break;
        case 1:
            rec = Gp_ItemTable1;
            break;
        default:
            rec = save->field_1AC;
            break;
    }
    j   = 0;
    rec = (GpItemRec*)((s32)rec + (dest->field_0 << 2));
    if (dest->field_1 != 0) {
        rows  = Gp_StatRows;
        save2 = &Mc_SaveData;
        attrs = Gp_ModStatAttrs;
        do {
            if ((s8)rec->field_1 == -1) {
                id = rec->field_0;
                if ((u32)(id - 0x60) < 0x20U) {
                    SCHED_BARRIER();
                    cfg->field_23   = id - 0x5F;
                    hpCfg           = &Wip_SysConfig;
                    hpVal           = rows[save2->field_F].field_0;
                    hpCfg->field_1a = hpVal;
                    hpVal          += save2->field_26;
                    hpCfg->field_1a = hpVal;
                    if (hpCfg->field_23 != 0) {
                        idx             = hpCfg->field_23;
                        idx             = idx - 1;
                        hpVal          += attrs[idx].field_4;
                        hpCfg->field_1a = hpVal;
                    }
                    if (hpCfg->field_1a >= 0xFB) {
                        hpCfg->field_1a = 0xFA;
                    }
                    if (hpCfg->field_18 > hpCfg->field_1a) {
                        hpCfg->field_18 = hpCfg->field_1a;
                    }
                    Gp_RecalcMaxMp();
                    break;
                }
            }
            j++;
            rec++;
        } while (j < dest->field_1);
    }

    state          = &Gp_StateC08;
    pcfg           = &Wip_SysConfig;
    hp             = pcfg->field_1a;
    mp             = pcfg->field_1e;
    state->field_B = 0;
    state->field_5 = 0;
    pcfg->field_18 = hp;
    pcfg->field_1c = mp;
    Gp_ApplyItemMap();
}

void Gp_InitModeEquip(void)
{
    WipSysConfig*       cfg;
    GpItemScan*         scan;
    GpItemRec*          tmp;
    register GpItemRec* table asm("a1");
    GpItemRec*          rec;
    s32                 i;
    s32                 acc;
    s32                 count;
    s32                 start;
    s32                 limit;
    s32                 off;
    s32                 item;
    GpItemSlot*         slots;
    u8                  slotItem;

    cfg = &Wip_SysConfig;
    acc = 0;
    if (cfg->field_21 == 0) {
        scan = &Mc_SaveData.field_5BC;
        item = 0x81;
        switch (scan->field_2) {
            case 2:
                tmp = Gp_ItemTable2;
                break;
            case 1:
                tmp = Gp_ItemTable1;
                break;
            default:
                tmp = Mc_SaveData.field_1AC;
                break;
        }
        table = tmp;
        i     = 0;
        count = scan->field_1;
        start = scan->field_0;
        if (count != 0) {
            limit = count;
            off   = start << 2;
            rec   = (GpItemRec*)(off + (s32)table);
            do {
                if (rec->field_0 == item) {
                    acc += rec->field_2;
                }
                i++;
                rec++;
            } while (i < limit);
        }
        if (acc != 0) {
            Gp_EquipHeld(0x81);
        }
    }
    if (cfg->field_21 == 2) {
        slots    = Mc_SaveData.field_1C8;
        slotItem = slots[0x81].field_0;
        if ((slotItem == 0) || (slotItem == 0xA0)) {
            Gp_EquipRelatedItem(&Mc_SaveData.field_5BC, 0x81, 0xA0, -1);
        }
    }
}

void Gp_ApplyBit2Bank(s32 arg0)
{
    GpBit2List*  table;
    GpBit2Rec*   rec;
    u32*         dest;
    register u32 id asm("a0");
    register u32 word asm("v1");
    register u32 val asm("a1");
    register s32 tmp asm("v0");
    register s32 three asm("t2");
    register u16 term asm("t3");
    u16          inner;
    GpBit2Bank*  bank;

    tmp  = (s32)Gp_Bit2Banks;
    bank = (GpBit2Bank*)tmp + arg0;
    tmp  = 3;
    val  = (u32)bank->field_0;
    dest = bank->field_4;
    if (arg0 == tmp) {
        return;
    }
    table = (GpBit2List*)val;
    if (table == NULL) {
        return;
    }
    rec = table->field_0;
    if (rec == (GpBit2Rec*)-1) {
        return;
    }
    term  = 0xFFFF;
    three = 3;
    do {
        if (rec != NULL) {
            id  = rec->field_0;
            tmp = id & 0xF;
            if (id != term) {
                inner = 0xFFFF;
                do {
                    tmp       = tmp * 2;
                    tmp       = three << tmp;
                    id        = (u32)(dest + (id >> 4));
                    word      = *(u32*)id;
                    word     &= ~tmp;
                    *(u32*)id = word;
                    val       = rec->field_6;
                    tmp       = rec->field_0;
                    rec++;
                    val      &= 3;
                    tmp      &= 0xF;
                    tmp      *= 2;
                    tmp       = val << tmp;
                    word     |= tmp;
                    *(u32*)id = word;
                    id        = rec->field_0;
                    tmp       = id & 0xF;
                } while (id != inner);
            }
        }
        table++;
        rec = table->field_0;
        tmp = -1;
    } while (rec != (GpBit2Rec*)tmp);
}

void Gp_SetCurBit2Flag(s32 arg0, u8 arg1)
{
    register s32  shift asm("a2");
    register u32  mask asm("a3");
    register u32* p asm("v1");
    register u32  nmask asm("a0");
    register s32  temp asm("v0");

    shift = (arg0 & 0xF) * 2;
    USE_REG(shift);
    temp  = 3;
    mask  = temp << shift;
    temp  = Mc_SaveData.field_7;
    p     = Gp_Bit2Banks[temp].field_4;
    p    += arg0 >> 4;
    nmask = ~mask;
    temp  = *p;
    mask  = arg1 << shift;
    *p    = (temp & nmask) | mask;
}

void Gp_ClearScanItems(GpItemScan* arg0)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("v1");
    register s32        i asm("a1");
    s32                 count;
    s32                 start;
    register s32        off asm("v0");

    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    i     = 0;
    count = arg0->field_1;
    start = arg0->field_0;
    if (count != 0) {
        off   = start << 2;
        table = (GpItemRec*)(off + (s32)table);
        do {
            i++;
            table->field_0 = 0;
            table->field_1 = 0;
            table->field_2 = 0;
            table++;
        } while (i < arg0->field_1);
    }
    USE_REG(i);
}

GpItemRec* Gp_GiveItem(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    return Gp_AddItem(arg0, arg1, arg2);
}

s32 Gp_RemoveItem(GpItemScan* arg0, GpItemRec* arg1, s32 arg2)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("v1");
    register s32        qty asm("t0");
    register GpItemRec* base asm("t1");
    s32                 item;
    GpItemRec*          rec;
    s32                 i;
    s32                 count;
    s32                 end;
    s32                 loop_end;

    item = arg1->field_0;
    if (item < 0xA0) {
        arg1->field_0 = 0;
        arg1->field_2 = 0;
        arg1->field_1 = 0;
    } else {
        switch (arg0->field_2) {
            case 2:
                tmp = Gp_ItemTable2;
                break;
            case 1:
                tmp = Gp_ItemTable1;
                break;
            found:
                qty = rec->field_2;
                goto after_loop;
            default:
                tmp = Mc_SaveData.field_1AC;
                break;
        }
        table = tmp;
        qty   = 0;
        USE_REG(qty);
        i     = arg0->field_0;
        count = arg0->field_1;
        base  = table;
        end   = i + count;
        if (i < end) {
            loop_end = end;
            rec      = (GpItemRec*)((i << 2) + (s32)base);
        loop:
            if (rec->field_0 != item) {
                i++;
                rec++;
                if (i < loop_end) {
                    goto loop;
                }
            } else {
                goto found;
            }
        }
    after_loop:
        if (i != arg0->field_0 + arg0->field_1) {
            if (arg2 < 0) {
                arg2 = qty;
            }
            arg2 = qty - arg2;
            if (arg2 < 0) {
                arg2 = 0;
            }
            if (arg2 == 0) {
                base[i].field_0 = 0;
                base[i].field_2 = 0;
                base[i].field_1 = 0;
            } else {
                base[i].field_2 = arg2;
            }
        }
    }
    return 0;
}

void Gp_ClearCollectedBits(void)
{
    s32  i;
    s32* p;

    p = Mc_SaveData.field_5AC;
    for (i = 3; i >= 0; i--) {
        *p++ = 0;
    }
}

void Gp_SetCollectedBit(s32 arg0)
{
    s32* p;
    s32  bit;

    p    = Mc_SaveData.field_5AC;
    bit  = arg0 & 0x7F;
    p   += bit / 32;
    bit %= 32;
    *p  |= 1 << bit;
    if ((arg0 & 0x7F) == 0x19) {
        Gp_PlayTimeMark = Mc_SaveData.field_C;
    }
}

void Gp_ClearCollectedBit(s32 arg0)
{
    s32* p;

    p     = Mc_SaveData.field_5AC;
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

    p     = Mc_SaveData.field_5AC;
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

s32 Gp_CountScanItems(GpItemScan* arg0)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("a3");
    GpItemRec*          rec;
    s32                 i;
    s32                 ret;
    s32                 count;
    s32                 start;
    s32                 limit;
    s32                 off;

    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    i     = 0;
    count = arg0->field_1;
    start = arg0->field_0;
    ret   = i;
    if (count != 0) {
        limit = count;
        off   = start << 2;
        rec   = (GpItemRec*)(off + (s32)table);
        do {
            if (rec->field_0 != 0) {
                ret++;
            }
            i++;
            rec++;
        } while (i < limit);
    }
    return ret;
}

GpItemSlot* Gp_GetItemSlot(s32 arg0)
{
    return &Mc_SaveData.field_1C8[arg0];
}

s32 Gp_CountEquippedRelated(GpItemScan* arg0, s32 arg1)
{
    GpItemRec*   table;
    s32          start;
    s32          count;
    s32          end;
    s32          limit;
    GpItemRec*   rec;
    GpItemSlot*  slots;
    GpItemSlot*  slot;
    GpItemSlot*  alt;
    s32          itemId;
    s32          off;
    register s32 ret asm("v0");

    table = Gp_GetItemTable(arg0);
    count = 0;
    if ((u32)(arg1 - 0xA0) < 0x20U) {
        start = arg0->field_0;
        end   = start + arg0->field_1;
        if (start < end) {
            slots = Mc_SaveData.field_1C8;
            limit = end;
            off   = start << 2;
            rec   = (GpItemRec*)(off + (s32)table);
            for (; start < limit; start++, rec++) {
                itemId = rec->field_0;
                if ((u32)(itemId - 0x80) < 0x20U) {
                    slot = (GpItemSlot*)((itemId << 3) + (s32)slots);
                    alt  = slot;
                    if (slot->field_0 == arg1) {
                        count += slot->field_1;
                    }
                    if (alt->field_2 == arg1) {
                        count += alt->field_3;
                    }
                }
            }
        }
    }
    ret = count;
    TOUCH_REG(ret);
    return ret;
}

void Gp_ClearEquipSlot(s32 arg0)
{
    GpItemSlot*  slot;
    register s32 found asm("a3");
    s32          i;
    GpItemMap*   p;

    if ((u32)(arg0 - 0x80) >= 0x20) {
        return;
    }

    found = 0;
    slot  = &Mc_SaveData.field_1C8[arg0];
    for (i = found, p = Gp_ItemMaps; i < 8; i++, p++) {
        if (arg0 == p->field_1) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
        slot->field_0 = 0;
        slot->field_1 = 0;
    }

    if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
        if (slot->field_2 != 0xFF) {
            slot->field_2 = 0;
        }
        slot->field_3 = 0;
    }
}

void Gp_ClearEquipSlotSel(s32 arg0, s32 arg1)
{
    GpItemSlot*  slot;
    register s32 found asm("t0");
    s32          i;
    GpItemMap*   p;

    if ((u32)(arg0 - 0x80) >= 0x20) {
        return;
    }

    found = 0;
    slot  = &Mc_SaveData.field_1C8[arg0];
    for (i = found, p = Gp_ItemMaps; i < 8; i++, p++) {
        if (arg0 == p->field_1) {
            found = 1;
            break;
        }
    }

    if (arg1 != 2) {
        if ((found == 0) || (Gp_ItemMaps[i].field_0 != 0)) {
            slot->field_0 = 0;
            slot->field_1 = 0;
        }
    }

    if (arg1 != 1) {
        if ((found == 0) || (Gp_ItemMaps[i].field_0 != 1)) {
            if (slot->field_2 != 0xFF) {
                slot->field_2 = 0;
            }
            slot->field_3 = 0;
        }
    }
}

s32 Gp_ScanStackQty(GpItemScan* arg0, s32 arg1)
{
    s32        index;
    s32        ret;
    GpItemRec* table;

    index = arg0->field_0;
    table = Gp_GetItemTable(arg0);
    if ((u32)(arg1 - 0xA0) < 0x20) {
        ret = (s16)Gp_FindScanQty(table, arg0, &index, arg1);
    } else {
        ret = 0;
    }
    return ret;
}

void Gp_ConsumeScanQty(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("v1");
    register s32        qty asm("t1");
    register GpItemRec* base asm("t2");
    GpItemRec*          rec;
    s32                 i;
    s32                 count;
    s32                 end;
    s32                 loop_end;

    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        found:
            qty = rec->field_2;
            goto after_loop;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    qty   = 0;
    USE_REG(qty);
    i     = arg0->field_0;
    count = arg0->field_1;
    base  = table;
    end   = i + count;
    if (i < end) {
        loop_end = end;
        rec      = (GpItemRec*)((i << 2) + (s32)base);
    loop:
        if (rec->field_0 != arg1) {
            i++;
            rec++;
            if (i < loop_end) {
                goto loop;
            }
        } else {
            goto found;
        }
    }
after_loop:
    if (i != arg0->field_0 + arg0->field_1) {
        if (arg2 < 0) {
            arg2 = qty;
        }
        arg2 = qty - arg2;
        if (arg2 < 0) {
            arg2 = 0;
        }
        if (arg2 == 0) {
            base[i].field_0 = 0;
            base[i].field_2 = 0;
            base[i].field_1 = 0;
        } else {
            base[i].field_2 = arg2;
        }
    }
}

s32 Gp_FillRelated(s32 arg0, s32 arg1)
{
    GpItemSlot* slot;
    GpItemSlot* alt;
    s32         ret;

    slot = &Mc_SaveData.field_1C8[arg0];
    alt  = slot;
    if (arg1 != 0) {
        ret = Gp_EquipRelatedItem(&Mc_SaveData.field_5BC, arg0, slot->field_2, -1);
    } else {
        ret = Gp_EquipRelatedItem(&Mc_SaveData.field_5BC, arg0, alt->field_0, -1);
    }
    return ret;
}

s32 Gp_UnequipRelated(s32 arg0, s32 arg1)
{
    GpItemSlot* slot;
    GpItemSlot* alt;
    s32         ret;

    slot = &Mc_SaveData.field_1C8[arg0];
    alt  = slot;
    if (arg1 == 0) {
        ret = Gp_EquipRelatedItem(&Mc_SaveData.field_5BC, arg0, slot->field_0, 0);
    } else {
        ret = Gp_EquipRelatedItem(&Mc_SaveData.field_5BC, arg0, alt->field_2, 0);
    }
    return ret == 0;
}

s32 Gp_GetCurBit2Flag(s32 arg0)
{
    register u32* p asm("v1");
    u32           word;
    s32           shift;

    p     = Gp_Bit2Banks[Game_Session->field_7].field_4;
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

    p     = Mc_SaveData.field_5AC;
    arg0 &= 0x7F;
    p    += arg0 / 32;
    arg0 %= 32;
    val   = *p & (1 << arg0);
    return val != 0;
}

GpItemRec* Gp_GetItemTable(GpItemScan* arg0)
{
    switch (arg0->field_2) {
        case 2:
            return Gp_ItemTable2;
        case 1:
            return Gp_ItemTable1;
        default:
            return Mc_SaveData.field_1AC;
    }
}

s32 Gp_ScanIndexOf(GpItemScan* arg0, GpItemRec* arg1)
{
    GpItemRec*   table;
    register s32 i asm("a2");
    s32          ret;

    switch (arg0->field_2) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData.field_1AC;
            break;
    }

    ret    = -1;
    table += arg0->field_0;
    for (i = 0; i < arg0->field_1; i++) {
        if (table == arg1) {
            ret = i;
            break;
        }
        table++;
    }
    return ret;
}

GpItemRec* Gp_GetScanSlot(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    GpItemRec* table;

    switch (arg0->field_2) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData.field_1AC;
            break;
    }
    return &table[arg0->field_0 + arg1];
}

s32 Gp_GetScanItemId(GpItemScan* arg0, s32 arg1)
{
    GpItemRec* table;
    GpItemRec* rec;

    switch (arg0->field_2) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData.field_1AC;
            break;
    }
    rec = &table[arg0->field_0 + arg1];
    return rec->field_0;
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
            bits  = Mc_SaveData.field_5AC;
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

s32 Gp_SumScanQty(GpItemScan* arg0, s32 arg1)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("a3");
    GpItemRec*          rec;
    s32                 i;
    s32                 acc;
    s32                 count;
    s32                 start;
    s32                 limit;
    s32                 off;

    if (arg1 >= 0x100) {
        return Gp_HasCollectedBit(arg1);
    }

    acc = 0;
    switch (arg0->field_2) {
        case 2:
            tmp = Gp_ItemTable2;
            break;
        case 1:
            tmp = Gp_ItemTable1;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    i     = 0;
    count = arg0->field_1;
    start = arg0->field_0;
    if (count != 0) {
        limit = count;
        off   = start << 2;
        rec   = (GpItemRec*)(off + (s32)table);
        do {
            if (rec->field_0 == arg1) {
                acc += rec->field_2;
            }
            i++;
            rec++;
        } while (i < limit);
    }
    return acc;
}

void func_800BB7B4(Task* arg0)
{
    ((GameActorExt*)arg0->extra)->field_C = 0;
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
        p                   = &Mc_SaveData;
        p->field_6D0[word] &= ~bit;
        return;
    }
    p                   = &Mc_SaveData;
    p->field_6D0[word] |= bit;
}

void Gp_ApplyBit2List(GpBit2List* arg0, u32* dest)
{
    GpBit2List*  table;
    GpBit2Rec*   rec;
    register u32 id asm("a0");
    register u32 word asm("v1");
    register u32 val asm("a1");
    register s32 tmp asm("v0");
    register s32 three asm("t1");
    register u16 term asm("t3");
    u16          inner;

    table = arg0;
    if (table == NULL) {
        return;
    }
    rec = table->field_0;
    if (rec == (GpBit2Rec*)-1) {
        return;
    }
    term  = 0xFFFF;
    three = 3;
    do {
        if (rec != NULL) {
            id  = rec->field_0;
            tmp = id & 0xF;
            if (id != term) {
                inner = 0xFFFF;
                do {
                    tmp       = tmp * 2;
                    tmp       = three << tmp;
                    id        = (u32)(dest + (id >> 4));
                    word      = *(u32*)id;
                    word     &= ~tmp;
                    *(u32*)id = word;
                    val       = rec->field_6;
                    tmp       = rec->field_0;
                    rec++;
                    val      &= 3;
                    tmp      &= 0xF;
                    tmp      *= 2;
                    tmp       = val << tmp;
                    word     |= tmp;
                    *(u32*)id = word;
                    id        = rec->field_0;
                    tmp       = id & 0xF;
                } while (id != inner);
            }
        }
        table++;
        rec = table->field_0;
        tmp = -1;
    } while (rec != (GpBit2Rec*)tmp);
}

void Gp_SetBit2Flag(s32 arg0, u8 arg1, s32 arg2)
{
    register s32  shift asm("a3");
    register u32  mask asm("t0");
    register u32* p asm("v1");
    register u32  nmask asm("a0");
    register s32  temp asm("v0");

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

s32 Gp_GetBit2Flag(GameSessionFrom4* arg0, s32 arg1)
{
    register u32* p asm("v1");
    u32           word;
    s32           shift;

    p     = Gp_Bit2Banks[arg0->field_3].field_4;
    p    += arg1 >> 4;
    shift = (arg1 & 0xF) * 2;
    word  = *p;
    USE_REG(arg0);
    return (word & (3 << shift)) >> shift;
}

void Gp_SavePlayerPos(void)
{
    GpCoordYaw*   coord;
    WipSysPos*    p;
    s32           angle;
    s32           temp;
    WipSysConfig* cfg;
    McSaveData*   save;

    coord      = (GpCoordYaw*)((GameActorExt*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    temp       = coord->field_18;
    p          = &Wip_SysConfig.field_10;
    p->field_0 = temp;
    p->field_2 = coord->field_1C;
    p->field_4 = coord->field_20;
    angle      = ratan2(coord->field_8, coord->field_14);
    p->field_6 = angle;
    if ((s16)angle >= 0x801) {
        p->field_6 = angle - 0x1000;
    } else if ((s16)angle < -0x800) {
        p->field_6 = angle + 0x1000;
    }
    cfg            = &Wip_SysConfig;
    save           = &Mc_SaveData;
    save->field_14 = cfg->field_8;
    save->field_18 = cfg->field_C;
}

GpEnemy* Gp_SpawnAtPlace(GpEnemyDesc* arg0, GpEnemyPlace* arg1)
{
    GpEnemy*      enemy;
    Task*         task;
    GameActorExt* extra;
    GpCoordPlace* coord;

    enemy = Gp_SpawnEnemyFromTable(&arg0->field_4, 0, arg0->field_0, NULL);
    if (enemy != NULL) {
        task = enemy->task;
        if (task->spawnType != 0) {
            extra             = (GameActorExt*)task->extra;
            coord             = (GpCoordPlace*)extra->field_8;
            enemy->field_8    = arg1->field_0 | (arg1->field_4 << 8);
            enemy->field_A    = arg1->field_2;
            coord->coord.t[0] = arg1->field_8;
            coord->coord.t[1] = arg1->field_A;
            coord->coord.t[2] = arg1->field_C;
            coord->field_46   = arg1->field_E;
            if (coord->field_46 != 0) {
                Gfx_RotMatrixY(&coord->coord, (s16)arg1->field_E, 1);
            }
            coord->flg = 0;
        }
    }
    return enemy;
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBB54);

void Gp_WaitItemFlag2(Task* arg0)
{
    GameActorExt* extra;

    extra = arg0->extra;
    if (arg0->state == 0) {
        extra->field_C = 8;
        arg0->state   += 1;
    }
    if (arg0->state == 1) {
        register GpBit2Bank* banks asm("v0");
        register u32*        p asm("v1");
        GameSession*         sess;
        s32                  id;
        s32                  shift;
        u32                  word;

        sess  = Game_Session;
        banks = Gp_Bit2Banks;
        id    = ((GpItemObj8*)arg0->spawnArg2)->field_8;
        p     = banks[sess->field_7].field_4;
        p    += id >> 4;
        shift = (id & 0xF) * 2;
        word  = *p;
        if (((word & (3 << shift)) >> shift) == 2) {
            extra->field_C &= 0xFFF7;
            Task_CallExit(arg0);
        }
    }
}

s32 Gp_FindScanQty(GpItemRec* arg0, GpItemScan* arg1, s32* arg2, s32 arg3)
{
    s32 i;
    s32 ret;
    s32 next;

    i   = *arg2;
    ret = 0;
    if (i < arg1->field_0 + arg1->field_1) {
    loop:
        if (arg0[i].field_0 == arg3) {
            ret = arg0[i].field_2;
        } else {
            next  = i + 1;
            *arg2 = next;
            i     = next;
            if (i < arg1->field_0 + arg1->field_1) {
                goto loop;
            }
        }
    }
    return (s16)ret;
}

s32 Gp_NextMappedSlot(s32 arg0)
{
    GpItemScan* scan;
    s32         i;
    GpItemMap*  p;

    scan = &Mc_SaveData.field_5BC;
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
    GpItemScan* scan;
    s32         i;
    GpItemMap*  p;

    found = 0;
    scan  = &Mc_SaveData.field_5BC;
    for (i = 0, p = Gp_ItemMaps; i < 8; i++) {
        if (Gp_SumScanQty(scan, p->field_1)) {
            found = 1;
            break;
        }
        p++;
    }
    return found;
}

void Gp_ResetAuxSlots(void)
{
    GpItemSlot* p;
    s32         i;

    p = Mc_SaveData.field_5C8;
    for (i = 0; i < 0x20; i++) {
        p->field_0 = 0;
        p->field_1 = 0;
        p->field_2 = 0xFF;
        p->field_3 = 0;
        if (i == 0x1A) {
            p->field_2 = 0;
            p->field_3 = 0;
        }
        p->field_4 = 0;
        p++;
    }
    Gp_ApplyItemMap();
}

s32 Gp_SumItemQty(s32 arg0)
{
    GpItemScan query;

    memset(&query, 0, sizeof(query));
    query.field_1 = 0xFF;
    return Gp_SumScanQty(&query, arg0);
}

void Gp_SetPlayerScan(s32 arg0)
{
    McSaveData* p;

    p                    = &Mc_SaveData;
    p->field_5BC.field_0 = 0;
    p->field_5BC.field_1 = arg0;
    p->field_5BC.field_2 = 0;
}

void Gp_SyncHeldRelated(void)
{
    WipSysConfig* p;
    GpItemSlot*   slots;
    s32           idx;
    u8            item;

    p = &Wip_SysConfig;
    if (p->field_21 == 0) {
        p->field_22 = 0;
    } else {
        slots = Mc_SaveData.field_1C8;
        idx   = p->field_21 + 0x7F;
        item  = slots[idx].field_0;
        if (item == 0) {
            p->field_22 = 0;
        } else {
            p->field_22 = item + 0x61;
        }
    }
    func_801061F0();
}

void Gp_InitItemSeenBits(void)
{
    McSaveData* p;
    GpItemDesc* desc;
    u8*         str;
    s32         i;
    s32         count;

    p = &Mc_SaveData;
    for (i = 0x5F; i >= 0; i--) {
        p->field_6D0[i] = 0;
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
    p   = &Mc_SaveData;
    val = p->field_6D0[word] & bit;
    return val != 0;
}

void Gp_RecalcMaxHp(void)
{
    WipSysConfig* cfg;
    McSaveData*   save;
    GpStatRow*    table;
    u16           val;

    cfg           = &Wip_SysConfig;
    table         = Gp_StatRows;
    save          = &Mc_SaveData;
    val           = table[save->field_F].field_0;
    cfg->field_1a = val;
    val          += save->field_26;
    cfg->field_1a = val;
    if (cfg->field_23 != 0) {
        val          += Gp_ModStatAttrs[cfg->field_23 - 1].field_4;
        cfg->field_1a = val;
    }
    if (cfg->field_1a >= 0xFB) {
        cfg->field_1a = 0xFA;
    }
    if (cfg->field_18 > cfg->field_1a) {
        cfg->field_18 = cfg->field_1a;
    }
}

void Gp_FillHpMp(void)
{
    WipSysConfig* p;

    p           = &Wip_SysConfig;
    p->field_18 = p->field_1a;
    p->field_1c = p->field_1e;
}

s32 Gp_GetScanCount(u8* arg0)
{
    return arg0[1];
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
    Gp_PlayTimeMark = Mc_SaveData.field_C;
}

s16 Gp_PlayTimeDelta(void)
{
    u16* p;

    p = &Gp_PlayTimeMark;
    return Mc_SaveData.field_C - *p;
}

s32 Gp_AgeFlag119(void)
{
    s32  ret;
    u16* p;

    ret = 0;
    if (Gp_HasCollectedBit(0x119) != 0) {
        p = &Gp_PlayTimeMark;
        if ((s16)(Mc_SaveData.field_C - *p) >= 2) {
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
        if ((s16)(Mc_SaveData.field_C - *p) >= 2) {
            Gp_ClearCollectedBit(0x119);
            Gp_SetCollectedBit(0x11A);
        }
    }
}

s32 Gp_GetModLevel(s32 arg0)
{
    register s32 ret asm("a1");
    register s32 idx asm("a2");
    GpItemAttr*  p;

    idx = arg0 - 0x60;
    ret = 0;
    if ((u32)idx < 0x20) {
        p    = &Gp_ItemAttrs[arg0];
        ret  = p->field_5;
        ret += Mc_SaveData.field_908[idx];
        USE_REG(idx);
        if (ret >= 0xB) {
            ret = 0xA;
        }
    }
    return ret;
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
    GpItemScan* scan;
    GpItemRec*  table;
    s32         i;
    s32         ret;
    s32         count;

    scan = &Mc_SaveData.field_5BC;
    ret  = 0;
    switch (scan->field_2) {
        case 2:
            table = Gp_ItemTable2;
            break;
        case 1:
            table = Gp_ItemTable1;
            break;
        default:
            table = Mc_SaveData.field_1AC;
            break;
    }
    i      = 0;
    table += scan->field_0;
    count  = scan->field_1;
    for (; i < count; i++) {
        if ((s8)table->field_1 > 0) {
            if (table->field_0 == arg0) {
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

    p            = &Mc_SaveData;
    p->field_5BC = Gp_DefaultScan;
}

void func_800BC4BC(void)
{
    Wip_SysConfig.field_26 = 1;
    Gp_InitModeEquip();
}

void func_800BC4E4(void)
{
    Wip_SysConfig.field_26 = 2;
    Gp_InitModeEquip();
}

s32 Gp_CanMoveItems(void)
{
    register s32         ret asm("s4");
    register GpItemScan* src asm("s3");
    register GpItemRec*  table asm("s5");
    register s32         flag asm("s7");
    register GpItemRec*  rec asm("s0");
    register s32         count asm("s1");
    register s32         i asm("s2");
    register s32         off asm("v0");
    register u32         destHi asm("s6");
    s32                  start;

    src   = &Gp_MoveScanSrc;
    ret   = 0;
    table = Gp_GetItemTable(src);
    start = Gp_MoveScanSrc.field_0;
    count = Gp_CountScanItems(src + 1);
    flag  = ret;
    if (Gp_CountScanItems(src) <= 0) {
        return ret;
    }
    TOUCH_REG(ret);
    i = ret;
    if (ret < src->field_1) {
        destHi = 0x80110000; /* %hi(Gp_MoveScanDst); must precede the rec address */
        off    = start << 2;
        rec    = (GpItemRec*)(off + (s32)table);
        do {
            if (rec->field_0 != 0) {
                if ((u8)(rec->field_0 + 0x60) < 0x20) {
                    if (Gp_FindItemInScan(rec->field_0, (GpItemScan*)(destHi + (s32)(s16)0xD62C)) == 0) {
                        count++;
                    }
                } else {
                    count++;
                }
            }
            i++;
            rec++;
        } while (i < src->field_1);
    }
    USE_REG(table);
    if (flag != 0) {
        return ret;
    }
    if (Gp_MoveScanDst.field_1 >= count) {
        ret = 1;
    }
    return ret;
}
