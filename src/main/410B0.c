#include "common.h"

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/410B0", func_800508B0);

void func_800509B4(void)
{
    u32  i;
    s32* ptr;

    ptr = (s32*)D_8007EBF0;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 0x1C0U);
    D_8007EBE4 = NULL;
    D_8007EBE8 = NULL;
    D_8007EBE0 = 1;
}

GStruct16* func_800509F4(void)
{
    s32        i;
    s32        flag;
    GStruct16* ptr;

    i    = 0;
    flag = 1;
    for (ptr = D_8007EBF0; i < 0x40; i++, ptr++) {
        if (ptr->field_0 == 0) {
            ptr->field_0 = flag;
            ptr->field_2 = 0;
            return ptr;
        }
    }
    return NULL;
}

void func_80050A38(GStruct16* arg0)
{
    GStruct16* temp;

    if (arg0 != NULL) {
        D_8007EBE0 = 0;
        if (D_8007EBE4 == NULL) {
            D_8007EBE8     = arg0;
            D_8007EBE4     = arg0;
            arg0->field_14 = NULL;
        } else {
            temp           = D_8007EBE8;
            D_8007EBE8     = arg0;
            arg0->field_14 = temp;
            temp->field_18 = arg0;
        }
        arg0->field_18 = NULL;
        D_8007EBE0     = 1;
    }
}

void func_80050A90(GStruct16* arg0)
{
    if (arg0 != NULL) {
        arg0->field_0  = 0;
        arg0->field_14 = NULL;
        arg0->field_18 = NULL;
    }
}

void func_80050AAC(void)
{
}

void func_80050AB4(GStruct16* arg0)
{
    func_80050E3C(arg0->field_4, arg0->field_6);
}

void func_80050AE0(GStruct16* arg0)
{
    func_800515C0(arg0->field_4, arg0->field_6);
}

void func_80050B0C(GStruct16* arg0)
{
    func_8005166C(arg0->field_4, 1);
}

void func_80050B30(GStruct16* arg0)
{
    func_8005166C(arg0->field_4, 0);
}

void func_80050B54(GStruct16* arg0)
{
    func_80051744(arg0->field_4, arg0->field_5);
}

void func_80050B80(GStruct16* arg0)
{
    GStruct16From4* temp;

    temp = (GStruct16From4*)&arg0->field_4;
    func_800558E8(temp->field_4, arg0->field_4, temp->field_1, temp->field_8, temp->field_C);
}

void func_80050BBC(GStruct16* arg0)
{
    GStruct16From4* temp;

    temp = (GStruct16From4*)&arg0->field_4;
    func_800546F4(temp->field_4, temp->field_2);
}

void func_80050BE8(GStruct16* arg0)
{
    func_800559BC(arg0->field_8, 1);
}

void func_80050C0C(GStruct16* arg0)
{
    func_800559BC(arg0->field_8, 0);
}

void func_80050C30(GStruct16* arg0)
{
    s32             temp_v0;
    GStruct16From4* temp_s0;

    temp_s0 = (GStruct16From4*)&arg0->field_4;
    temp_v0 = func_80055DAC(temp_s0->field_4);
    if (temp_v0 >= 0) {
        func_80055A9C(temp_v0, arg0->field_4, temp_s0->field_1);
    }
}

void func_80050C80(GStruct16* arg0)
{
    s32             temp_v0;
    GStruct16From4* temp_s0;

    temp_s0 = (GStruct16From4*)&arg0->field_4;
    temp_v0 = func_80055DAC(temp_s0->field_4);
    if (temp_v0 >= 0) {
        func_80055B70(temp_v0, temp_s0->field_1);
    }
}

void func_80050CC0(void)
{
    func_80055C00();
}

void func_80050CE0(void)
{
    func_80055C8C();
}

void func_80050D00(void)
{
    func_80054F1C();
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80050D20);

INCLUDE_ASM("main/nonmatchings/410B0", func_80050E3C);

INCLUDE_ASM("main/nonmatchings/410B0", func_800510D4);

s32 func_800512BC(s32 arg0, s32 arg1)
{
    GStruct16* temp;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = func_800509F4();
    if (temp == NULL) {
        return -2;
    }
    temp->field_2 = 1;
    temp->field_4 = arg0;
    temp->field_6 = arg1;
    func_80050A38(temp);
    return 0;
}

s32 func_8005132C(s32 arg0, s32 arg1)
{
    GStruct16* temp;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = func_800509F4();
    if (temp == NULL) {
        return -2;
    }
    temp->field_2 = 2;
    temp->field_4 = arg0;
    temp->field_6 = arg1 & 0xFFFC;
    func_80050A38(temp);
    return 0;
}

s32 func_800513A0(s32 arg0)
{
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

s32 func_80051400(s32 arg0)
{
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

s32 func_80051460(s32 arg0, s32 arg1)
{
    GStruct16*      temp;
    GStruct16From4* mid;

    if ((arg0 & 0xFF) == 0xFF) {
        return -3;
    }
    temp = func_800509F4();
    if (temp == NULL) {
        return -2;
    }
    temp->field_4 = arg0;
    temp->field_2 = 5;
    mid           = (GStruct16From4*)&temp->field_4;
    if ((s8)arg1 >= 0) {
        mid->field_1 = arg1;
    } else {
        mid->field_1 = 0x7F;
    }
    func_80050A38(temp);
    D_800820E8 = mid->field_1;
    return 0;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_800514F8);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051560);

INCLUDE_ASM("main/nonmatchings/410B0", func_800515C0);

INCLUDE_ASM("main/nonmatchings/410B0", func_8005166C);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051744);

void func_800517B4(s32 arg0)
{
    s32 i;
    s32 val;
    u8* flag;

    flag = &D_8007F2F0;
    if ((s8)arg0 >= 0) {
        *flag = arg0;
    } else {
        *flag = 0x7F;
    }

    i   = 0;
    val = 0xFFFF;
    for (; i <= 0; i++) {
        (&D_8007F300)[i].field_C = val;
    }
}

s32 func_800517F8(void)
{
    return D_8007F2F0;
}

u8* func_80051808(void)
{
    if (D_8007F300.field_0 != 0) {
        func_80051AB8(&D_8007F300);
        D_8007F300.field_0 = 4;
    }
    return (u8*)&D_8007F300;
}

void* func_80051850(s32 arg0, s32 arg1)
{
    return D_8007F8E0;
}

void func_8005185C(s32* arg0)
{
    u32  i;
    s32* ptr;

    ptr = arg0;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 3U);
    ((s8*)arg0)[1] = -1;
    ((s8*)arg0)[0] = -1;
}

void func_80051888(void)
{
    GStruct16*      temp;
    GStruct16From4* mid;

    D_800820E9 = 1;
    temp       = func_800509F4();
    if (temp != NULL) {
        mid           = (GStruct16From4*)&temp->field_4;
        temp->field_2 = 5;
        temp->field_4 = 0;
        mid->field_1  = 0;
        func_80050A38(temp);
        D_800820E8 = mid->field_1;
    }
}

void func_800518E0(void)
{
    GStruct16*      temp;
    GStruct16From4* mid;
    u8              saved;

    if (D_800820E9 != 0) {
        saved      = D_800820E8;
        D_800820E9 = 0;
        temp       = func_800509F4();
        if (temp != NULL) {
            mid           = (GStruct16From4*)&temp->field_4;
            temp->field_2 = 5;
            temp->field_4 = 0;
            if ((s8)saved >= 0) {
                mid->field_1 = saved;
            } else {
                mid->field_1 = 0x7F;
            }
            func_80050A38(temp);
            D_800820E8 = mid->field_1;
        }
    }
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80051964);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051A2C);

void func_80051AB8(GStruct36* arg0)
{
    s32 i;

    for (i = 0; i < arg0->field_3; i++) {
        arg0->entries[i].field_5 = 1;
    }
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80051AF0);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051BB0);

INCLUDE_ASM("main/nonmatchings/410B0", func_80051DF4);

INCLUDE_ASM("main/nonmatchings/410B0", func_800520A8);

INCLUDE_ASM("main/nonmatchings/410B0", func_80052488);

INCLUDE_ASM("main/nonmatchings/410B0", func_800526A4);

s32 func_8005287C(u8* arg0, u8* arg1)
{
    register s32 result asm("a2");

    result = 0;
    *arg1  = 0;
    do {
        result <<= 7;
        result  |= *arg0 & 0x7F;
        *arg1    = *arg1 + 1;
    } while (*arg0++ & 0x80);
    return result;
}

void func_800528BC(s32* arg0)
{
    s32 i;

    if (arg0 != NULL) {
        for (i = 0; i < 0x10; i++) {
            *arg0++ = 0x407F4000;
            *arg0++ = 0;
        }
    }
}

s32 func_800528F0(s32 arg0, s32 arg1)
{
    return arg1 + 1;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_800528F8);

u8* func_800529BC(s32 arg0, u8* arg1, GStruct22* arg2)
{
    arg2->field_484[arg0 & 0xF].field_4 = arg1[1];
    return arg1 + 2;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_800529D8);

INCLUDE_ASM("main/nonmatchings/410B0", func_80052B30);

INCLUDE_ASM("main/nonmatchings/410B0", func_80052F80);

INCLUDE_ASM("main/nonmatchings/410B0", func_800530DC);

void func_8005325C(void* arg0)
{
    func_8005363C(8, arg0);
}

void func_80053280(u8 arg0, void* arg1)
{
    D_800820F3 = arg0;
    D_8008212C = D_80082122;
    D_80082121 = D_80082135;
    func_8005363C(0, arg1);
}

void func_800532CC(void)
{
    GStruct34* temp;

    D_80082122 = D_8008212C;
    D_80082135 = D_80082121;
    temp       = &D_800820F0;
    if (temp->field_2 != 6) {
        temp->field_2 = 8;
        F3D458_Free((void*)temp->field_14);
        temp->field_14 = 0;
        func_8004D0F0((GStruct42*)temp->field_18);
        temp->field_18 = 0;
    }
}

s32 func_8005333C(void* arg0)
{
    GStruct34* temp_s1;
    s32        temp_s0;

    if (D_80068A78 != 0) {
        return -1;
    }
    temp_s1 = &D_800820F0;
    if (temp_s1->field_3 != 0) {
        temp_s1->field_10 = 0x800;
    } else {
        switch (temp_s1->field_2) {
            case 0:
            case 1:
            case 3:
                temp_s1->field_10 = 0x7F0;
                arg0              = (u8*)arg0 + 0x10;
                break;
            case 2:
            case 4:
            case 7:
                temp_s1->field_10 = 0x800;
                break;
            case 5:
                return 5;
            case 8:
                return 0;
        }
    }
    temp_s0 = func_80052B30(arg0);
    if (temp_s0 == 7) {
        return -1;
    }
    if (temp_s0 == 5) {
        func_800530DC(temp_s1);
    }
    return temp_s0;
}

s32 func_80053414(void* arg0)
{
    s32 temp;

    temp = func_80052B30(arg0);
    if (temp == 7) {
        return -1;
    }
    return temp;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_80053448);

void* func_80053548(s32 arg0, s32 arg1, u32 arg2)
{
    u16 x;

    x = arg0;
    if ((arg1 & 0xFF) == 0) {
        return func_80051850(0, arg2 & 0xFFFF);
    }
    if (D_800680AC[x >> 12] == -1) {
        return 0;
    }
    switch (arg0 & 0xF000) {
        case 0x2000:
            if (arg2 < 0x210U) {
                arg2 = 0x210;
            }
            break;
        case 0xE000:
            if (arg2 < 0x168U) {
                arg2 = 0x168;
            }
            break;
    }
    return F3D458_Malloc(arg2);
}

s32 func_800535F0(s32 arg0, s32 arg1, s32 arg2)
{
    s32 result;

    arg0  &= 3;
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

void func_8005363C(s32 arg0, void* arg1)
{
    GStruct34* temp;
    s32        size;

    D_800689E8 = 0;
    temp       = &D_800820F0;
    if (arg0 == 8) {
        size          = 0x800;
        temp->field_0 = arg0;
    } else {
        size          = 0x7F0;
        temp->field_0 = 0;
    }
    temp->field_10 = size;
    temp->field_2  = 0;
    temp->field_1  = 0;
    temp->field_4  = arg1;
    temp->field_14 = 0;
    temp->field_18 = 0;
    temp->field_8  = 0;
    temp->field_C  = 0;
}

INCLUDE_ASM("main/nonmatchings/410B0", func_8005368C);
