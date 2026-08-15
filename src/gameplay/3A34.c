#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"

s32 func_800B715C(GpItemScan* arg0, s32 arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D5B14);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6170);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6334);

s32 func_800D68C4(s32 arg0)
{
    s32 val;

    val = D_8010F88C;
    if (val <= 0) {
        if (val >= 0) {
            return -1;
        }
        val = -val;
    }
    D_8010F88C = 0;
    return func_800B715C(&D_80072724, arg0, val, -1);
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6910);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6994);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6A24);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6AA4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6B20);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6E5C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D70E4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D72D0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D759C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D78A4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D7A9C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8684);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8C0C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8EA0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9138);

void func_800D930C(GpObj4C* arg0, s32 arg1)
{
    u8 val;

    val   = arg0->field_4E;
    arg1 &= 3;
    if ((val & 3) != arg1) {
        arg0->field_4E = (val & 0xF0) | ((val & 3) << 2) | arg1;
        arg0->field_4F = 0x10;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9340);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D937C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D94B8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9504);

void func_800D9550(GpObj20* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    MATRIX* m;

    m       = arg0->field_20;
    m->t[0] = arg1;
    m->t[1] = arg2;
    m->t[2] = arg3;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D957C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9618);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9654);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D96C8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9718);

s32 func_800D9788(GpObj38* arg0)
{
    return arg0->field_38;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9794);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D98C4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9A30);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9B9C);

void func_800D9C3C(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    arg0->field_0.vx = arg0->field_0.vy = arg0->field_0.vz = arg1;
    arg0->field_6.vx = arg0->field_6.vy = arg0->field_6.vz = arg2;
    arg0->field_C.vx = arg0->field_C.vy = arg0->field_C.vz = arg3;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9C64);

void func_800D9CC8(Task* arg0)
{
    Task_CallExit(arg0);
}

void func_800D9CE8(GBytes8* arg0)
{
    *arg0 = D_8010F9E4;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9D18);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9DFC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA2A0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA6E8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA7B8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAB38);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DABEC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAC54);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DACAC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DACF8);

void* func_800DAD54(GpActorWork* arg0)
{
    VECTOR3 pos;

    return func_800DA2A0(arg0, &pos, 0);
}

void* func_800DAD78(GpActorWork* arg0)
{
    VECTOR3  pos;
    VECTOR3* p;
    s32      flag;

    p = &pos;
    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return func_800DA2A0(arg0, p, flag);
}

void* func_800DADE4(GpActorWork* arg0, VECTOR3* pos)
{
    s32 flag;

    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return func_800DA2A0(arg0, pos, flag);
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAE50);

void func_800DAF98(void)
{
    s32       i;
    GpSlot70* p;

    p = D_80115270;
    i = 0;
    do {
        i++;
        p->field_0 = NULL;
        p->field_4 = 0;
        p->field_6 = 0;
        p++;
    } while (i < 0x20);
}

void func_800DAFD0(void)
{
    D_80115268 = NULL;
    func_800DAF98();
    D_8010F9F0 = 0xFFF00000;
    D_8010F9EC = 0xFFF00000;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB004);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB0D8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB128);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB28C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB31C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB3FC);

void func_800DB4E0(void)
{
    if (D_801153F0.field_0 == 0) {
        D_801153F0.field_0 = 1;
    }
}

void func_800DB500(s32 arg0)
{
    if (arg0 != 0) {
        D_801153F0.field_2 |= 1 << (arg0 - 1);
    }
}

void func_800DB530(s32 arg0)
{
    D_801153F3 = arg0;
}

void func_800DB53C(void)
{
    D_801153F0.field_6++;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB558);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB630);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB6B4);

void func_800DB72C(void)
{
    if (Game_GetPtrSlot(3) != NULL) {
        func_8010154C();
        func_800E0540(D_80115570);
        func_800E0540(D_80115574);
        func_800E0540(D_80115578);
        func_800E0540(D_8011557C);
        func_800E0540(D_80115580);
        func_800E0540(D_8011558C);
        func_800E0540(D_80115590);
        func_800E0414(D_80115570, D_80115578);
        func_800E0414(D_80115570, D_8011557C);
        func_800E0414(D_80115570, D_80115580);
        func_800E0414(D_80115570, D_80115590);
        func_800DB900(D_80115570);
        func_800E0414(D_80115574, D_80115578);
        func_800E0414(D_80115574, D_80115580);
        func_800E0414(D_80115574, D_80115588);
        func_800E0414(D_80115578, D_80115580);
        func_800E0414(D_80115578, D_80115590);
        func_800DB900(D_80115578);
        func_800E0414(D_8011557C, D_80115580);
        func_800E0414(D_80115580, D_80115590);
        if (D_80115424 != 0) {
            func_800E0B08();
        }
        func_800E0608(D_80115570, 0x9007, 0x9004);
        if (Game_Session->field_12C == 0) {
            func_800E06AC(D_80115570, 0xA007, 0xA004);
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB900);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBA20);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBCAC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBE7C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DC528);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DCB80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD324);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD940);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DDC2C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DDDF8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE150);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE2C0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE7CC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEAFC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEC80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEF80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DF6AC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DFCCC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0294);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0308);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0414);

void func_800E0540(GpObj* node)
{
    u16 flags;

    if (D_80115448 != 0) {
        for (; node != NULL; node = node->next) {
            flags = node->flags;
            if (flags & 0x4000) {
                switch (flags & 7) {
                    case 0:
                        break;
                    case 1:
                        func_800DC528(node);
                        break;
                    case 2:
                        break;
                    case 3:
                        func_800DDDF8(node);
                        break;
                    case 4:
                        if (node->flags & 0x200) {
                            func_800DD940(node);
                        }
                        func_800DCB80(node);
                        break;
                }
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0608);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E06AC);

s32 func_800E076C(void)
{
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0774);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E08CC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0994);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0B08);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0B48);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0C10);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0FEC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1380);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E15AC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1638);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1688);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1708);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1758);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E17B4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1834);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1884);

void func_800E18E0(GpRec18* arg0, s32 arg1)
{
    Mem_Set(arg0, 0, arg1 * 0x18);
    arg0[arg1 - 1].field_0 = 2;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E192C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E19B8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1A1C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1A6C);

s32 func_800E1ACC(u8* arg0)
{
    s32 val;
    s32 ret;

    val = *arg0 << 12;
    if (val != 0) {
        ret = cln(val) / 2839;
    } else {
        ret = 0;
    }
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1B24);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1B80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1BF0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1C58);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1CD4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1FEC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2438);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E25F8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2A24);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2BF8);

s32 func_800E2C40(GpU16Pair* arg0, s32 arg1)
{
    s32 ret;

    if (arg0 == NULL) {
        return 0;
    }
    ret  = arg0[arg1].field_0 & 0xFFF;
    ret |= (arg0[arg1].field_2 & 0xF) << 12;
    ret |= 0x40000;
    return ret;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2C78);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2CD4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2D3C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2D90);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2DE4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2EC4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2F7C);

void func_800E3008(GpObj4C* arg0)
{
    arg0->field_4C |= 1;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E301C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E3084);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E3194);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E31E8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E337C);
