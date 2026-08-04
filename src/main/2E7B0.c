#include "common.h"

#include "main/game.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003DFB0);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E210);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E324);

GStruct0* func_8003E438(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GStruct0Node* saved;
    GStruct0*     ret;

    saved = func_8002D40C();
    func_8002D41C(&D_800716E0);
    ret = func_8002CFDC(arg0, arg1, arg2, arg3);
    func_8002D41C(saved);
    return ret;
}

GStruct0* func_8003E4BC(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GStruct0Node* saved;
    GStruct0*     ret;

    saved = func_8002D40C();
    func_8002D41C(&D_800716E0);
    ret = func_8002CFA0(arg0, arg1, arg2, arg3);
    func_8002D41C(saved);
    return ret;
}

void func_8003E540(void)
{
    func_8003E814();
}

void func_8003E560(void)
{
    GStruct1* temp;
    u_long*   saved;
    s32       buf;
    u_long*   ot;

    temp            = &D_80070F68;
    saved           = D_800710A0;
    buf             = temp->field_114 ^ 1;
    temp->field_114 = buf;
    D_800710A0      = D5F414_OrderingTables + buf * C5F414_OTAG_ENTRIES;
    ClearOTagR(D_800710A0, C5F414_OTAG_ENTRIES);
    ot         = D_800710A0;
    *ot        = C5F414_OTAG_END_PRIM;
    D_800710A0 = ot + 0x20;
    func_800AC688();
    func_8009850C(&D_80070EE8[temp->field_114]);
    D_800710A0      = saved;
    temp->field_103 = 0;
}

void func_8003E610(void)
{
    GStruct1* temp;

    temp = &D_80070F68;
    if (temp->field_1d >= 0) {
        temp->field_1d |= 0x80;
        temp->field_11d = 1;
    } else {
        temp->field_11d++;
    }
}

void func_8003E64C(void)
{
    GStruct1* temp;
    u8        val;

    temp = &D_80070F68;
    if (temp->field_1d >= 0) {
        temp->field_11d = 0;
    } else {
        val             = temp->field_11d - 1;
        temp->field_11d = val;
        if (val == 0) {
            temp->field_1d &= 0x7F;
            temp->field_11d = 0;
        }
    }
}

s32 func_8003E698(void)
{
    s32 temp;

    temp = D_8007A118;
    if (temp == 2) {
        goto case2;
    }
    if (temp < 3) {
        goto default_case;
    }
    if (temp == 3) {
        goto case3;
    }
    goto default_case;
case2:
    return 4;
case3:
    return 6;
default_case:
    return D_80070F68.field_1d;
}

void func_8003E6E4(void)
{
    GStruct50* ot;

    ot           = D_8007A0E8;
    ot->length   = 6;
    ot->org      = D_8007A120;
    ot[1].length = 6;
    ot[1].org    = D_8007A120 + 0x40;
    D_8007A0E0   = D_800740E0;
    D_8007A0E4   = 0x6000;
}

s32 func_8003E72C(s32 arg0)
{
    if (arg0 >= 0x20) {
        if (arg0 < 0x80) {
            D_80070F68.field_10d = 0;
            if (arg0 != 0x43) {
                func_8003FB70(&D_8006268C, arg0, 0, 0);
            } else {
                func_8003FB70(&D_8006268C, 0x43, 0, 0);
            }
            D_80070F68.field_10d = arg0;
            if (D_80070F68.field_12c != 0) {
                func_8003FA3C(0xFF);
                func_8003F86C(0, 0, 0x10, 1);
            } else if (arg0 != 0x42) {
                if (arg0 == 0x43) {
                    func_8003FA3C(0xFF);
                    func_8003F86C(0, 0, 0x20, 1);
                } else {
                    func_8003FA3C(0x20);
                    func_8003F86C(0, 0, 8, 1);
                }
            }
        }
        func_8003F690();
    }
    return 0;
}

void func_8003E814(void)
{
    GStruct14* temp;

    temp = D4F564_8005ED64;
    func_800144F8(temp->field_7, temp->field_6);
    D_80070F68.field_1e  = 0;
    D_80070F68.field_10d = 0;
}

void func_8003E854(void)
{
    GStruct1* temp;
    u_long*   saved;
    s32       buf;

    temp            = &D_80070F68;
    saved           = D_800710A0;
    buf             = temp->field_114 ^ 1;
    temp->field_114 = buf;
    D_800710A0      = D5F414_OrderingTables + buf * C5F414_OTAG_ENTRIES;
    F179D4_ClearOTag(temp->field_114);
    D_800710A0 = D_800710A0 + 0x20;
    func_8002D544(&D_800716E0, 0x62);
    func_80097AC0(&D_80070EE8[temp->field_114]);
    D_800710A0      = saved;
    temp->field_103 = 0;
}

void func_8003E904(void)
{
    GStruct50* ot;
    GStruct1*  temp;
    u_long*    org;

    ot           = D_8007A0E8;
    ot->length   = 0xA;
    ot->org      = D5F414_OrderingTables;
    ot[1].length = 0xA;
    ot[1].org    = D5F414_OrderingTables + C5F414_OTAG_ENTRIES;
    temp         = &D_80070F68;
    GsClearOt(0, 0, &ot[temp->field_118]);
    org        = ot[temp->field_118].org;
    *org       = C5F414_OTAG_END_PRIM;
    D_800710A0 = org;
}

void func_8003E9A4(void)
{
    D_8007A0E4 = 0x10000;
    D_8007A0E0 = (void*)D_80068F88;
}

void func_8003E9C4(void)
{
    D_8007A0E0 = D_800740E0;
    D_8007A0E4 = 0x6000;
}
