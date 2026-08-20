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
#include "main/ui.h"
#include "main/wipsys.h"

extern u16          D_800739B8;
extern GpItemRec*   D_80114DD4;
extern UiObjectDesc D_8010D384;

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
s32  func_800B715C(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_800CF448(s32 arg0);
void func_800B6CF0(void);
void func_800BAEC0(s32 arg0);
void func_800BAE5C(s32 arg0);
s32  func_800BBCCC(GpItemRec* arg0, GpItemScan* arg1, s32* arg2, s32 arg3);
void func_800C1148(UiPanel* arg0, s32 arg1);
void func_801061F0(void);
void func_800D2F68(Task* arg0);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B7420);

void func_800B7930(void)
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
    table  = D_8011398C;
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
            asm volatile("" ::"r"(start));
        }
        levels++;
        i++;
        base += 3;
    }
    if (cfg->field_23 != 0) {
        acc += D_8010E2B8[cfg->field_23 - 1].field_6;
    }
    rows          = D_8010D328;
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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B7A50);

void func_800B7D18(void)
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
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
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
                    func_800BAD08(dest, 0x3D, 1);
                } else if (item == 0x8A) {
                    func_800BAD08(dest, 0x3C, 1);
                } else if (item == 0x65) {
                    func_800BAD08(dest, 0xD, 1);
                } else if ((item != 0x81) && (item != 0xA0) && (item != 0x60) &&
                           (item != 0x40) && (item != 0x92)) {
                    func_800BAD08(dest, rec->field_0, rec->field_2);
                }
            }
            i++;
            rec++;
        } while (i < scan->field_1);
    }
    func_800BAC8C(scan);
    scans = D_8010D550;
    func_800BAC8C(scans[1]);
    func_800BAC8C(scans[2]);
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
    func_800B6CF0();
    func_800BAD08(scan, 0x63, 1);
    Game_Session->field_11C = -1;
    cfg->field_21           = 0;
    cfg->field_26           = three;
    func_800B7A50(0x63);
    added          = func_800BAD08(scan, 0x40, 1);
    added->field_1 = 1;
    added          = func_800BAD08(scan, 2, 1);
    added->field_1 = 2;
    added          = func_800BAD08(scan, 0x81, 1);
    added->field_1 = three;
    func_800BAD08(scan, 0xA0, 0x64);
    func_800B715C(scan, 0x81, 0xA0, -1);
    func_800BAD08(scan, 0x92, 1);
    cfg2           = &Wip_SysConfig;
    hp             = cfg2->field_1a;
    mp             = cfg2->field_1e;
    cfg2->field_18 = hp;
    cfg2->field_1c = mp;
    flag105        = func_800BB4BC(0x105);
    flag107        = func_800BB4BC(0x107);
    func_800BAE38();
    if (flag105 != 0) {
        func_800BAE5C(0x105);
    }
    if (flag107 != 0) {
        func_800BAE5C(0x107);
    }
    func_800BAE5C(0x106);
    func_800BAE5C(0x10C);
    func_800BAE5C(0x10B);
    func_800BAE5C(0x10A);
    func_800BAE5C(0x109);
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B8014);

void func_800B83F0(GpItemScan* arg0, s32 arg1, s32 arg2)
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
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B8588);

s32 func_800B87F4(GpItemScan* arg0, s32 arg1, s32 arg2)
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
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
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
                table2 = D_80114C20;
                break;
            case 1:
                table2 = D_80114D70;
                break;
            default:
                table2 = Mc_SaveData.field_1AC;
                break;
        }
        if (arg2 < 0) {
            arg2 = D_8010E3B8[arg1 - 0xA0].field_0;
        }
        i      = 0;
        count2 = arg0->field_1;
        if (count2 != 0) {
            p      = D_8010E3B8;
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

s32 func_800B8988(GpItemScan* arg0, s32 arg1)
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
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
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
                table2 = D_80114C20;
                break;
            case 1:
                table2 = D_80114D70;
                break;
            default:
                table2 = Mc_SaveData.field_1AC;
                break;
        }
        i      = 0;
        count2 = arg0->field_1;
        if (count2 != 0) {
            p      = D_8010E3B8;
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

GpItemRec* func_800B8B00(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3)
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
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    if ((u32)(arg2 - 0xA0) < 0x20U) {
        dest = func_800BAD08(arg0, arg2, arg3);
        i    = arg0->field_0;
        if (((GpItemRec*)(((i + arg1) << 2) + (s32)table))->field_0 != 0) {
            goto done;
        }
        start = i;
        asm volatile("" ::"r"(start));
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
    asm volatile("" ::"r"(field0));
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
    func_800BAD08(arg0, item, qty);
done:
    return dest;
}

GpItemRec* func_800B8CAC(GpItemScan* arg0, s32 arg1, s32 arg2)
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
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
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
                arg2 = D_8010E3B8[temp].field_2;
            } else {
                arg2 = D_8010E3B8[temp].field_0;
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
            attrs = D_8010E3B8;
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
        asm("" : "+r"(found));
        i     = 0;
        attrs = D_8010E3B8;
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

char* func_800B8EB0(s32 arg0, s32 arg1, s32 arg2)
{
    s8*          str;
    GpItemDesc*  table;
    GpItemDesc*  desc;
    s32          bit;
    s32          val;
    s8*          prev;
    register s32 tmp asm("v1");

    if (arg0 >= 0x500) {
        str = D_801D6484[arg0 - 0x500];
    } else if (arg0 >= 0x300) {
        tmp  = arg0 & 3;
        arg2 = (arg0 & 0xF0) >> 4;
        arg0 = (arg0 & 0xC) >> 2;
        if (tmp == 0) {
            tmp = 1;
        }
        arg0 = (arg2 * 3 + arg0) * 3;
        val  = tmp + 0xE;
        str  = func_800B8EB0(arg0 + val, arg1, 1);
    } else {
        tmp = arg0 << 3;
        if (arg0 < 0x100) {
            table = D_8010D838;
        } else {
            table = D_8010D638;
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

s32 func_800B904C(GpItemScan* arg0, s32 arg1, s32 arg2)
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
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
            break;
        default:
            tmp = Mc_SaveData.field_1AC;
            break;
    }
    table = tmp;
    idx   = arg0->field_0;
    cfg   = &Wip_SysConfig;
    if (arg1 >= 0) {
        table0 = D_8010E238;
        table1 = D_8010D278;
        temp   = idx << 2;
        rec    = (GpItemRec*)(temp + (s32)table);
        do {
            if ((u8)(rec->field_0 + 0x80) < 0x20) {
                rec2 = rec;
                if (arg2 == 0) {
                    goto decrement;
                }
                i = 0;
                asm volatile("" ::"r"(i));
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

void func_800B91C8(GpItemRec* arg0)
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
    for (i = found, p = D_8010D2F8; i < 8; i++, p++) {
        if (item == p->field_1) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (D_8010D2F8[i].field_0 != 0)) {
        slot->field_0 = 0;
        slot->field_1 = 0;
    }

    if ((found == 0) || (D_8010D2F8[i].field_0 != 1)) {
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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B954C);

static __inline void func_800B996C_RemoveItem(GpItemScan* arg0, GpItemRec* arg1, s32 arg2)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("v1");
    register s32        qty asm("a2");
    register GpItemRec* base asm("t0");
    s32                 item;
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
                tmp = D_80114C20;
                break;
            case 1:
                tmp = D_80114D70;
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
        asm volatile("" ::"r"(qty));
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

void func_800B996C(UiObject* arg0, Task* arg1)
{
    WipSysConfig* cfg;
    McSaveData*   save;
    s32           saved;

    if (arg1->state == 0) {
        cfg                = &Wip_SysConfig;
        D_80114BE8.field_0 = cfg->field_18;
        save               = &Mc_SaveData;
        D_80114BE8.field_4 = cfg->field_1c;
        if (save->field_27 < 0xFA) {
            save->field_27 = save->field_27 + 1;
        }
        func_800B7930();
        cfg->field_1c = cfg->field_1e;
        func_800B996C_RemoveItem(0, D_80114DD4, 1);
        Ui_SpawnFromDesc(&D_8010D384, 0, 0, 1, arg0);
    }
    saved           = arg1->spawnArg1;
    arg1->spawnArg1 = 0x1D;
    func_800D2F68(arg1);
    arg1->spawnArg1 = saved;
}

void func_800B9B40(UiObject* arg0, Task* arg1)
{
    WipSysConfig* cfg;
    McSaveData*   save;
    s32           saved;
    s32           hp;
    u16           val;

    if (arg1->state == 0) {
        cfg                = &Wip_SysConfig;
        hp                 = cfg->field_18;
        D_80114BE8.field_0 = hp;
        save               = &Mc_SaveData;
        D_80114BE8.field_4 = cfg->field_1c;
        if (save->field_26 < 0xFA) {
            save->field_26 = save->field_26 + 5;
        }
        val           = D_8010D328[save->field_F].field_0;
        cfg->field_1a = val;
        val          += save->field_26;
        cfg->field_1a = val;
        if (cfg->field_23 != 0) {
            val          += D_8010E2B8[cfg->field_23 - 1].field_4;
            cfg->field_1a = val;
        }
        if (cfg->field_1a >= 0xFB) {
            cfg->field_1a = 0xFA;
        }
        if (cfg->field_1a < hp) {
            cfg->field_18 = cfg->field_1a;
        }
        cfg->field_18 = cfg->field_1a;
        func_800B996C_RemoveItem(0, D_80114DD4, 1);
        Ui_SpawnFromDesc(&D_8010D384, 0, 0, 1, arg0);
    }
    saved           = arg1->spawnArg1;
    arg1->spawnArg1 = 0x1C;
    func_800D2F68(arg1);
    arg1->spawnArg1 = saved;
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B9D80);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BA538);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BA75C);

void func_800BAA58(void)
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
                tmp = D_80114C20;
                break;
            case 1:
                tmp = D_80114D70;
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
            func_800CF448(0x81);
        }
    }
    if (cfg->field_21 == 2) {
        slots    = Mc_SaveData.field_1C8;
        slotItem = slots[0x81].field_0;
        if ((slotItem == 0) || (slotItem == 0xA0)) {
            func_800B715C(&Mc_SaveData.field_5BC, 0x81, 0xA0, -1);
        }
    }
}

void func_800BAB64(s32 arg0)
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

    tmp  = (s32)D_8010D230;
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

void func_800BAC34(s32 arg0, u8 arg1)
{
    register s32  shift asm("a2");
    register u32  mask asm("a3");
    register u32* p asm("v1");
    register u32  nmask asm("a0");
    register s32  temp asm("v0");

    shift = (arg0 & 0xF) * 2;
    asm volatile("" ::"r"(shift));
    temp  = 3;
    mask  = temp << shift;
    temp  = Mc_SaveData.field_7;
    p     = D_8010D230[temp].field_4;
    p    += arg0 >> 4;
    nmask = ~mask;
    temp  = *p;
    mask  = arg1 << shift;
    *p    = (temp & nmask) | mask;
}

void func_800BAC8C(GpItemScan* arg0)
{
    GpItemRec*          tmp;
    register GpItemRec* table asm("v1");
    register s32        i asm("a1");
    s32                 count;
    s32                 start;
    register s32        off asm("v0");

    switch (arg0->field_2) {
        case 2:
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
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
    asm volatile("" ::"r"(i));
}

GpItemRec* func_800BAD08(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    return func_800B8CAC(arg0, arg1, arg2);
}

s32 func_800BAD28(GpItemScan* arg0, GpItemRec* arg1, s32 arg2)
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
                tmp = D_80114C20;
                break;
            case 1:
                tmp = D_80114D70;
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
        asm volatile("" ::"r"(qty));
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

void func_800BAE38(void)
{
    s32  i;
    s32* p;

    p = Mc_SaveData.field_5AC;
    for (i = 3; i >= 0; i--) {
        *p++ = 0;
    }
}

void func_800BAE5C(s32 arg0)
{
    s32* p;
    s32  bit;

    p    = Mc_SaveData.field_5AC;
    bit  = arg0 & 0x7F;
    p   += bit / 32;
    bit %= 32;
    *p  |= 1 << bit;
    if ((arg0 & 0x7F) == 0x19) {
        D_800739B8 = Mc_SaveData.field_C;
    }
}

void func_800BAEC0(s32 arg0)
{
    s32* p;

    p     = Mc_SaveData.field_5AC;
    arg0 &= 0x7F;
    p    += arg0 / 32;
    arg0 %= 32;
    *p   &= ~(1 << arg0);
}

s32 func_800BAF08(void)
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

s32 func_800BAF5C(GpItemScan* arg0)
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
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
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

GpItemSlot* func_800BAFE0(s32 arg0)
{
    return &Mc_SaveData.field_1C8[arg0];
}

s32 func_800BAFF4(GpItemScan* arg0, s32 arg1)
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

    table = func_800BB500(arg0);
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
    asm volatile("" : "+r"(ret));
    return ret;
}

void func_800BB0CC(s32 arg0)
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
    for (i = found, p = D_8010D2F8; i < 8; i++, p++) {
        if (arg0 == p->field_1) {
            found = 1;
            break;
        }
    }

    if ((found == 0) || (D_8010D2F8[i].field_0 != 0)) {
        slot->field_0 = 0;
        slot->field_1 = 0;
    }

    if ((found == 0) || (D_8010D2F8[i].field_0 != 1)) {
        if (slot->field_2 != 0xFF) {
            slot->field_2 = 0;
        }
        slot->field_3 = 0;
    }
}

void func_800BB190(s32 arg0, s32 arg1)
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
    for (i = found, p = D_8010D2F8; i < 8; i++, p++) {
        if (arg0 == p->field_1) {
            found = 1;
            break;
        }
    }

    if (arg1 != 2) {
        if ((found == 0) || (D_8010D2F8[i].field_0 != 0)) {
            slot->field_0 = 0;
            slot->field_1 = 0;
        }
    }

    if (arg1 != 1) {
        if ((found == 0) || (D_8010D2F8[i].field_0 != 1)) {
            if (slot->field_2 != 0xFF) {
                slot->field_2 = 0;
            }
            slot->field_3 = 0;
        }
    }
}

s32 func_800BB26C(GpItemScan* arg0, s32 arg1)
{
    s32        index;
    s32        ret;
    GpItemRec* table;

    index = arg0->field_0;
    table = func_800BB500(arg0);
    if ((u32)(arg1 - 0xA0) < 0x20) {
        ret = (s16)func_800BBCCC(table, arg0, &index, arg1);
    } else {
        ret = 0;
    }
    return ret;
}

void func_800BB2D4(GpItemScan* arg0, s32 arg1, s32 arg2)
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
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
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
    asm volatile("" ::"r"(qty));
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

s32 func_800BB3C0(s32 arg0, s32 arg1)
{
    GpItemSlot* slot;
    GpItemSlot* alt;
    s32         ret;

    slot = &Mc_SaveData.field_1C8[arg0];
    alt  = slot;
    if (arg1 != 0) {
        ret = func_800B715C(&Mc_SaveData.field_5BC, arg0, slot->field_2, -1);
    } else {
        ret = func_800B715C(&Mc_SaveData.field_5BC, arg0, alt->field_0, -1);
    }
    return ret;
}

s32 func_800BB418(s32 arg0, s32 arg1)
{
    GpItemSlot* slot;
    GpItemSlot* alt;
    s32         ret;

    slot = &Mc_SaveData.field_1C8[arg0];
    alt  = slot;
    if (arg1 == 0) {
        ret = func_800B715C(&Mc_SaveData.field_5BC, arg0, slot->field_0, 0);
    } else {
        ret = func_800B715C(&Mc_SaveData.field_5BC, arg0, alt->field_2, 0);
    }
    return ret == 0;
}

s32 func_800BB470(s32 arg0)
{
    register u32* p asm("v1");
    u32           word;
    s32           shift;

    p     = D_8010D230[Game_Session->field_7].field_4;
    p    += arg0 >> 4;
    shift = (arg0 & 0xF) * 2;
    word  = *p;
    asm volatile("" ::: "a1");
    return (word & (3 << shift)) >> shift;
}

s32 func_800BB4BC(s32 arg0)
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

GpItemRec* func_800BB500(GpItemScan* arg0)
{
    switch (arg0->field_2) {
        case 2:
            return D_80114C20;
        case 1:
            return D_80114D70;
        default:
            return Mc_SaveData.field_1AC;
    }
}

s32 func_800BB540(GpItemScan* arg0, GpItemRec* arg1)
{
    GpItemRec*   table;
    register s32 i asm("a2");
    s32          ret;

    switch (arg0->field_2) {
        case 2:
            table = D_80114C20;
            break;
        case 1:
            table = D_80114D70;
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

GpItemRec* func_800BB5BC(GpItemScan* arg0, s32 arg1, s32 arg2)
{
    GpItemRec* table;

    switch (arg0->field_2) {
        case 2:
            table = D_80114C20;
            break;
        case 1:
            table = D_80114D70;
            break;
        default:
            table = Mc_SaveData.field_1AC;
            break;
    }
    return &table[arg0->field_0 + arg1];
}

s32 func_800BB610(GpItemScan* arg0, s32 arg1)
{
    GpItemRec* table;
    GpItemRec* rec;

    switch (arg0->field_2) {
        case 2:
            table = D_80114C20;
            break;
        case 1:
            table = D_80114D70;
            break;
        default:
            table = Mc_SaveData.field_1AC;
            break;
    }
    rec = &table[arg0->field_0 + arg1];
    return rec->field_0;
}

s32 func_800BB668(s32 arg0, s32 arg1)
{
    u16* p;
    s32  ret;
    s32* bits;
    s32  bit;
    s32  item;
    s32  one;

    p   = D_80114AE0;
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

s32 func_800BB6FC(GpItemScan* arg0, s32 arg1)
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
        return func_800BB4BC(arg1);
    }

    acc = 0;
    switch (arg0->field_2) {
        case 2:
            tmp = D_80114C20;
            break;
        case 1:
            tmp = D_80114D70;
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

void func_800BB7C0(s32 arg0, s32 arg1)
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

void func_800BB838(GpBit2List* arg0, u32* dest)
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

void func_800BB8E8(s32 arg0, u8 arg1, s32 arg2)
{
    register s32  shift asm("a3");
    register u32  mask asm("t0");
    register u32* p asm("v1");
    register u32  nmask asm("a0");
    register s32  temp asm("v0");

    shift = (arg0 & 0xF) * 2;
    asm volatile("" ::"r"(shift));
    temp  = 3;
    mask  = temp << shift;
    p     = D_8010D230[arg2].field_4;
    p    += arg0 >> 4;
    nmask = ~mask;
    temp  = *p;
    mask  = arg1 << shift;
    *p    = (temp & nmask) | mask;
}

s32 func_800BB938(s32 arg0, s32 arg1)
{
    s32 ret;

    arg0 -= 0x80;
    ret   = 0;
    if ((u32)arg0 < 0x20) {
        if (arg1 == 0) {
            ret = D_8010E238[arg0].field_0;
        } else {
            ret = D_8010D278[arg0].field_0;
        }
    }
    return ret;
}

s32 func_800BB974(GameSessionFrom4* arg0, s32 arg1)
{
    register u32* p asm("v1");
    u32           word;
    s32           shift;

    p     = D_8010D230[arg0->field_3].field_4;
    p    += arg1 >> 4;
    shift = (arg1 & 0xF) * 2;
    word  = *p;
    asm volatile("" ::"r"(arg0));
    return (word & (3 << shift)) >> shift;
}

void func_800BB9B8(void)
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

GpEnemy* func_800BBA70(GpEnemyDesc* arg0, GpEnemyPlace* arg1)
{
    GpEnemy*      enemy;
    Task*         task;
    GameActorExt* extra;
    GpCoordPlace* coord;

    enemy = func_800B01AC(&arg0->field_4, 0, arg0->field_0, NULL);
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

void func_800BBC10(Task* arg0)
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
        banks = D_8010D230;
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

s32 func_800BBCCC(GpItemRec* arg0, GpItemScan* arg1, s32* arg2, s32 arg3)
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

s32 func_800BBD40(s32 arg0)
{
    GpItemScan* scan;
    s32         i;
    GpItemMap*  p;

    scan = &Mc_SaveData.field_5BC;
    if ((u32)arg0 >= 8) {
        return -1;
    }
    for (i = arg0; i < 8; i++) {
        p = &D_8010D2F8[i];
        if (func_800BB6FC(scan, p->field_1)) {
            return i;
        }
    }
    return -1;
}

GpItemMap* func_800BBDC8(s32 arg0)
{
    return &D_8010D2F8[arg0];
}

s32 func_800BBDDC(void)
{
    s32         found;
    GpItemScan* scan;
    s32         i;
    GpItemMap*  p;

    found = 0;
    scan  = &Mc_SaveData.field_5BC;
    for (i = 0, p = D_8010D2F8; i < 8; i++) {
        if (func_800BB6FC(scan, p->field_1)) {
            found = 1;
            break;
        }
        p++;
    }
    return found;
}

void func_800BBE54(void)
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
    func_800B6CF0();
}

s32 func_800BBEC0(s32 arg0)
{
    GpItemScan query;

    memset(&query, 0, sizeof(query));
    query.field_1 = 0xFF;
    return func_800BB6FC(&query, arg0);
}

void func_800BBF04(s32 arg0)
{
    McSaveData* p;

    p                    = &Mc_SaveData;
    p->field_5BC.field_0 = 0;
    p->field_5BC.field_1 = arg0;
    p->field_5BC.field_2 = 0;
}

void func_800BBF1C(void)
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

void func_800BBF84(void)
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
            desc = &D_8010D838[i];
        } else {
            desc = &D_8010D638[i];
        }
        str = desc->field_4;
        while (count > 0) {
            if (*str == 0 || *str == 0xA) {
                count--;
            }
            str++;
        }
        if (*str == 0xA) {
            func_800BB7C0(i, 1);
        }
        i++;
    } while (i < 0x180);
}

s32 func_800BC06C(s32 arg0)
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

void func_800BC0C0(void)
{
    WipSysConfig* cfg;
    McSaveData*   save;
    GpStatRow*    table;
    u16           val;

    cfg           = &Wip_SysConfig;
    table         = D_8010D328;
    save          = &Mc_SaveData;
    val           = table[save->field_F].field_0;
    cfg->field_1a = val;
    val          += save->field_26;
    cfg->field_1a = val;
    if (cfg->field_23 != 0) {
        val          += D_8010E2B8[cfg->field_23 - 1].field_4;
        cfg->field_1a = val;
    }
    if (cfg->field_1a >= 0xFB) {
        cfg->field_1a = 0xFA;
    }
    if (cfg->field_18 > cfg->field_1a) {
        cfg->field_18 = cfg->field_1a;
    }
}

void func_800BC164(void)
{
    WipSysConfig* p;

    p           = &Wip_SysConfig;
    p->field_18 = p->field_1a;
    p->field_1c = p->field_1e;
}

s32 func_800BC180(u8* arg0)
{
    return arg0[1];
}

s32 func_800BC18C(s32 arg0)
{
    register s32 ret asm("v1");
    s32          idx;

    ret = 0;
    if (arg0 == 0) {
        ret = 0x1000;
    } else if ((u32)(arg0 - 1) < 0x5F) {
        ret = D_80114A40[arg0];
    } else {
        idx = arg0 - 0x60;
        if ((u32)idx < 0x20) {
            ret = D_80114A88[idx];
        } else {
            idx = arg0 - 0x80;
            if ((u32)idx < 0x20) {
                ret = D_80114A98[idx];
            } else {
                idx = arg0 - 0xA0;
                if ((u32)idx < 0x20) {
                    ret = D_80114ABC[idx];
                }
            }
        }
    }
    if (ret == 0) {
        ret = arg0 + 0x100;
    }
    return ret;
}

void func_800BC21C(void)
{
    D_800739B8 = Mc_SaveData.field_C;
}

s16 func_800BC230(void)
{
    u16* p;

    p = &D_800739B8;
    return Mc_SaveData.field_C - *p;
}

s32 func_800BC254(void)
{
    s32  ret;
    u16* p;

    ret = 0;
    if (func_800BB4BC(0x119) != 0) {
        p = &D_800739B8;
        if ((s16)(Mc_SaveData.field_C - *p) >= 2) {
            func_800BAEC0(0x119);
            func_800BAE5C(0x11A);
            ret = 1;
        }
    }
    return ret;
}

void func_800BC2C4(void)
{
    u16* p;

    if (func_800BB4BC(0x119) != 0) {
        p = &D_800739B8;
        if ((s16)(Mc_SaveData.field_C - *p) >= 2) {
            func_800BAEC0(0x119);
            func_800BAE5C(0x11A);
        }
    }
}

s32 func_800BC324(s32 arg0)
{
    register s32 ret asm("a1");
    register s32 idx asm("a2");
    GpItemAttr*  p;

    idx = arg0 - 0x60;
    ret = 0;
    if ((u32)idx < 0x20) {
        p    = &D_8010DFB8[arg0];
        ret  = p->field_5;
        ret += Mc_SaveData.field_908[idx];
        asm volatile("" ::"r"(idx));
        if (ret >= 0xB) {
            ret = 0xA;
        }
    }
    return ret;
}

void func_800BC378(Task* arg0)
{
    UiPanel* panel;

    panel = arg0->spawnArg2;
    if (arg0->state == 0) {
        Ui_UpdateLayoutSize(panel, 0xB0, 0x2F);
        panel->field_C.y = -0xC;
        panel->field_C.x = -panel->field_C.w / 2;
        arg0->state++;
    }
    func_800C1148(panel, 0);
}

s32 func_800BC3F8(s32 arg0)
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
            table = D_80114C20;
            break;
        case 1:
            table = D_80114D70;
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

void func_800BC490(void)
{
    McSaveData* p;

    p            = &Mc_SaveData;
    p->field_5BC = D_8010D520;
}

void func_800BC4BC(void)
{
    Wip_SysConfig.field_26 = 1;
    func_800BAA58();
}

void func_800BC4E4(void)
{
    Wip_SysConfig.field_26 = 2;
    func_800BAA58();
}

s32 func_800BC50C(void)
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

    src   = &D_8010D628;
    ret   = 0;
    table = func_800BB500(src);
    start = D_8010D628.field_0;
    count = func_800BAF5C(src + 1);
    flag  = ret;
    if (func_800BAF5C(src) <= 0) {
        return ret;
    }
    asm volatile("" : "+r"(ret));
    i = ret;
    if (ret < src->field_1) {
        destHi = 0x80110000; /* %hi(D_8010D62C); must precede the rec address */
        off    = start << 2;
        rec    = (GpItemRec*)(off + (s32)table);
        do {
            if (rec->field_0 != 0) {
                if ((u8)(rec->field_0 + 0x60) < 0x20) {
                    if (func_800D6A24(rec->field_0, (GpItemScan*)(destHi + (s32)(s16)0xD62C)) == 0) {
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
    asm volatile("" ::"r"(table));
    if (flag != 0) {
        return ret;
    }
    if (D_8010D62C.field_1 >= count) {
        ret = 1;
    }
    return ret;
}
