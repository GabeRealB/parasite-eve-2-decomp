#include "common.h"

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/410B0", func_800508B0);

INCLUDE_ASM("main/nonmatchings/410B0", func_800509B4);

INCLUDE_ASM("main/nonmatchings/410B0", func_800509F4);

INCLUDE_ASM("main/nonmatchings/410B0", func_80050A38);

INCLUDE_ASM("main/nonmatchings/410B0", func_80050A90);

void func_80050AAC(void)
{
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80050AB4);

void func_80050AE0(GStruct16* arg0) {
    func_800515C0(arg0->field_4, arg0->field_6);
}

void func_80050B0C(GStruct16* arg0) {
    func_8005166C(arg0->field_4, 1);
}

void func_80050B30(GStruct16* arg0) {
    func_8005166C(arg0->field_4, 0);
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80050B54);

INCLUDE_ASM("main/nonmatchings/410B0", func_80050B80);

void func_80050BBC(GStruct16* arg0) {
    GStruct16From4* temp;

    temp = (GStruct16From4*)&arg0->field_4;
    func_800546F4(temp->field_4, temp->field_2);
}

void func_80050BE8(GStruct16* arg0) {
    func_800559BC(arg0->field_8, 1);
}

void func_80050C0C(GStruct16* arg0) {
    func_800559BC(arg0->field_8, 0);
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80050C30);

void func_80050C80(GStruct16* arg0) {
    s32 temp_v0;
    GStruct16From4* temp_s0;

    temp_s0 = (GStruct16From4*)&arg0->field_4;
    temp_v0 = func_80055DAC(temp_s0->field_4);
    if (temp_v0 >= 0) {
        func_80055B70(temp_v0, temp_s0->field_1);
    }
}

void func_80050CC0(void) {
    func_80055C00();
}

void func_80050CE0(void) {
    func_80055C8C();
}

void func_80050D00(void) {
    func_80054F1C();
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80050D20);

INCLUDE_ASM("main/nonmatchings/410B0", func_80050E3C);

INCLUDE_ASM("main/nonmatchings/410B0", func_800510D4);

INCLUDE_ASM("main/nonmatchings/410B0", func_800512BC);

INCLUDE_ASM("main/nonmatchings/410B0", func_8005132C);

s32 func_800513A0(s32 arg0) {
    GStruct16* temp;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = func_800509F4();
    if (temp == NULL) {
        return -2;
    }
    temp->field_2 = 3;
    temp->field_4 = arg0;
    func_80050A38(temp);
    return 0;
}

s32 func_80051400(s32 arg0) {
    GStruct16* temp;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = func_800509F4();
    if (temp == NULL) {
        return -2;
    }
    temp->field_2 = 4;
    temp->field_4 = arg0;
    func_80050A38(temp);
    return 0;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80051460);

INCLUDE_ASM("main/nonmatchings/410B0", func_800514F8);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051560);

INCLUDE_ASM("main/nonmatchings/410B0", func_800515C0);

INCLUDE_ASM("main/nonmatchings/410B0", func_8005166C);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051744);

INCLUDE_ASM("main/nonmatchings/410B0", func_800517B4);

s32 func_800517F8(void) {
    return D_8007F2F0;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80051808);

void *func_80051850(void) {
    return D_8007F8E0;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_8005185C);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051888);

INCLUDE_ASM("main/nonmatchings/410B0", func_800518E0);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051964);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051A2C);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051AB8);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051AF0);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051BB0);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051DF4);

INCLUDE_ASM("main/nonmatchings/410B0", func_800520A8);

INCLUDE_ASM("main/nonmatchings/410B0", func_80052488);

INCLUDE_ASM("main/nonmatchings/410B0", func_800526A4);

INCLUDE_ASM("main/nonmatchings/410B0", func_8005287C);

INCLUDE_ASM("main/nonmatchings/410B0", func_800528BC);

s32 func_800528F0(s32 arg0, s32 arg1)
{
    return arg1 + 1;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_800528F8);

INCLUDE_ASM("main/nonmatchings/410B0", func_800529BC);

INCLUDE_ASM("main/nonmatchings/410B0", func_800529D8);

INCLUDE_ASM("main/nonmatchings/410B0", func_80052B30);

INCLUDE_ASM("main/nonmatchings/410B0", func_80052F80);

INCLUDE_ASM("main/nonmatchings/410B0", func_800530DC);

void func_8005325C(void* arg0) {
    func_8005363C(8, arg0);
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80053280);

INCLUDE_ASM("main/nonmatchings/410B0", func_800532CC);

INCLUDE_ASM("main/nonmatchings/410B0", func_8005333C);

INCLUDE_ASM("main/nonmatchings/410B0", func_80053414);

INCLUDE_ASM("main/nonmatchings/410B0", func_80053448);

INCLUDE_ASM("main/nonmatchings/410B0", func_80053548);

s32 func_800535F0(s32 arg0, s32 arg1, s32 arg2) {
    s32 result;

    arg0 &= 3;
    result = 0;
    switch (arg0) {
    case 0:
        result = 0x1010;
        break;
    case 2:
        result = func_80053A20(arg2, arg1 & 0xFFFF);
        break;
    }
    return result;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_8005363C);

INCLUDE_ASM("main/nonmatchings/410B0", func_8005368C);
