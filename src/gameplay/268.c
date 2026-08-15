#include "common.h"

#include <psyq/memory.h>

#include "gameplay/268.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"

extern u16          D_80072174;
extern u16          D_800739B8;
extern WipSysConfig D_80073B88;

void  func_80180804(void);
void  func_8017EA68(void);
void  func_80181468(void);
void  func_8017EA90(void);
void  func_8017E9E8(void);
void  func_80181364(void);
void  func_8017EA58(void);
void  func_8017E9F8(void);
void  func_8017EAE0(void);
void  func_8018138C(void);
void  func_8017EA74(void);
void  func_8017EA78(void);
void  func_8017EB2C(void);
void  func_8017EDE8(void);
void  func_8017EAB4(void);
void  func_8017EA64(void);
void  func_8017EC04(void);
void  func_8017EAC4(void);
void  func_8017EA60(void);
void* func_800B8CAC(void* arg0, s32 arg1, s32 arg2);
void  func_800BAA58(void);
s32   func_800B715C(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_800BAEC0(s32 arg0);
void  func_800BAE5C(s32 arg0);
s32   func_800BBCCC(GpItemRec* arg0, GpItemScan* arg1, s32* arg2, s32 arg3);
void  func_801061F0(void);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B7420);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B7930);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B7A50);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B7D18);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B8014);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B83F0);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B8588);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B87F4);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B8988);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B8B00);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B8CAC);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B8EB0);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B904C);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B91C8);

void func_800B92CC(void)
{
    switch (*(u32*)&D_8007216C & 0xFFFF0000) {
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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B996C);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B9B40);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800B9D80);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BA538);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BA75C);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAA58);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAB64);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAC34);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAC8C);

void* func_800BAD08(void* arg0, s32 arg1, s32 arg2)
{
    return func_800B8CAC(arg0, arg1, arg2);
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAD28);

void func_800BAE38(void)
{
    s32  i;
    s32* p;

    p = D_80072714;
    for (i = 3; i >= 0; i--) {
        *p++ = 0;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAE5C);

void func_800BAEC0(s32 arg0)
{
    s32* p;

    p     = D_80072714;
    arg0 &= 0x7F;
    p    += arg0 / 32;
    arg0 %= 32;
    *p   &= ~(1 << arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAF08);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAF5C);

GpItemSlot* func_800BAFE0(s32 arg0)
{
    return &D_80072330[arg0];
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAFF4);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB0CC);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB190);

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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB2D4);

s32 func_800BB3C0(s32 arg0, s32 arg1)
{
    GpItemSlot* slot;
    GpItemSlot* alt;
    s32         ret;

    slot = &D_80072330[arg0];
    alt  = slot;
    if (arg1 != 0) {
        ret = func_800B715C(&((GpItemBlock*)D_80072330)->scan, arg0, slot->field_2, -1);
    } else {
        ret = func_800B715C(&((GpItemBlock*)D_80072330)->scan, arg0, alt->field_0, -1);
    }
    return ret;
}

s32 func_800BB418(s32 arg0, s32 arg1)
{
    GpItemSlot* slot;
    GpItemSlot* alt;
    s32         ret;

    slot = &D_80072330[arg0];
    alt  = slot;
    if (arg1 == 0) {
        ret = func_800B715C(&((GpItemBlock*)D_80072330)->scan, arg0, slot->field_0, 0);
    } else {
        ret = func_800B715C(&((GpItemBlock*)D_80072330)->scan, arg0, alt->field_2, 0);
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

    p     = D_80072714;
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
            return D_80072314;
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
            table = D_80072314;
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

GpItemRec* func_800BB5BC(GpItemScan* arg0, s32 arg1)
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
            table = D_80072314;
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
            table = D_80072314;
            break;
    }
    rec = &table[arg0->field_0 + arg1];
    return rec->field_0;
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB668);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB6FC);

void func_800BB7B4(Task* arg0)
{
    ((GameActorExt*)arg0->extra)->field_C = 0;
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB7C0);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB838);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB8E8);

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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB9B8);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBA70);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBB54);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBC10);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBCCC);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBD40);

GpItemMap* func_800BBDC8(s32 arg0)
{
    return &D_8010D2F8[arg0];
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBDDC);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBE54);

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

    p            = &Mc_SaveData;
    p->field_5BC = 0;
    p->field_5BD = arg0;
    p->field_5BE = 0;
}

void func_800BBF1C(void)
{
    WipSysConfig* p;
    u8            item;

    p = &D_80073B88;
    if (p->field_21 == 0) {
        p->field_22 = 0;
    } else {
        item = D_80072330[p->field_21 + 0x7F].field_0;
        if (item == 0) {
            p->field_22 = 0;
        } else {
            p->field_22 = item + 0x61;
        }
    }
    func_801061F0();
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBF84);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC06C);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC0C0);

void func_800BC164(void)
{
    WipSysConfig* p;

    p           = &D_80073B88;
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
    D_800739B8 = D_80072174;
}

s16 func_800BC230(void)
{
    u16* p;

    p = &D_800739B8;
    return D_80072174 - *p;
}

s32 func_800BC254(void)
{
    s32  ret;
    u16* p;

    ret = 0;
    if (func_800BB4BC(0x119) != 0) {
        p = &D_800739B8;
        if ((s16)(D_80072174 - *p) >= 2) {
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
        if ((s16)(D_80072174 - *p) >= 2) {
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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC378);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC3F8);

void func_800BC490(void)
{
    McSaveData* p;

    p                           = &Mc_SaveData;
    *(GpItemScan*)&p->field_5BC = D_8010D520;
}

void func_800BC4BC(void)
{
    D_80073B88.field_26 = 1;
    func_800BAA58();
}

void func_800BC4E4(void)
{
    D_80073B88.field_26 = 2;
    func_800BAA58();
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC50C);
