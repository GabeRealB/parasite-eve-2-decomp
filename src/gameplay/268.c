#include "common.h"

#include "gameplay/268.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"

extern u32          D_8007216C;
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
    switch (D_8007216C & 0xFFFF0000) {
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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAE38);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAE5C);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAEC0);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAF08);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAF5C);

GpItemSlot* func_800BAFE0(s32 arg0)
{
    return &D_80072330[arg0];
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BAFF4);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB0CC);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB190);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB26C);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB2D4);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB3C0);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB418);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB470);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB4BC);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB500);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB540);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB5BC);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB610);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB668);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB6FC);

void func_800BB7B4(Task* arg0)
{
    ((GameActorExt*)arg0->extra)->field_C = 0;
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB7C0);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB838);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB8E8);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB938);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BB974);

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

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBEC0);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBF04);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBF1C);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BBF84);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC06C);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC0C0);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC164);

s32 func_800BC180(u8* arg0)
{
    return arg0[1];
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC18C);

void func_800BC21C(void)
{
    D_800739B8 = D_80072174;
}

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC230);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC254);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC2C4);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC324);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC378);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC3F8);

INCLUDE_ASM("gameplay/nonmatchings/268", func_800BC490);

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
