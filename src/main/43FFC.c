#include "common.h"

#include "main/unknown_syms.h"

void func_800537FC(s32 arg0, s32 arg1) {
    u8* var_s0;
    s32 var_a0;
    s32 var_v1;
    s32 temp_v1;

    D_8008274C = 0;
    func_800566A4();
    arg0 = arg0 & 0xFF;
    func_800546C0();
    func_800542D0(0x50000000, 1);
    func_800542D0(0x10000000, 1);
    func_800542D0(0xFF0D, 1);
    func_800542D0(0x20000000, 1);
    func_800542D0(0xE0000000, 1);
    arg1 = arg1 & 0xFF;
    D_80082120 = arg0;
    D_80082136 = arg1;
    func_800561EC(1);
    func_800561EC(7);

    var_v1 = 0;
    if (arg1 == 5) {
        if (arg0 == 4) {
            var_a0 = 3;
        } else {
            goto block_5;
        }
    } else {
block_5:
        do {
            if (D_80068A54[var_v1 + arg0 * 2] == arg1) {
                var_a0 = 2;
                goto block_done;
            }
            var_v1++;
        } while (var_v1 < 2);
        var_a0 = 1;
    }
block_done:
    func_80054608(var_a0);
    D_80082130 = 0x3D010;
    D_80082128 = 0;
    D_80082124 = D_80082128;

    temp_v1 = (s8)D_80082135;
    switch (temp_v1) {
    case 0:
        func_8004D0F0(&D_8007E158);
        func_800561EC(4);
    case 1:
        D_80082122 = 0;
        break;
    case 2:
        D_80082122 = 1;
        break;
    }
    var_s0 = D_8007E0F8;

    D_800820F0.field_14 = 0;
    D_800820F0.field_18 = 0;
    D_8008212C = D_80082122;
    D_80082121 = D_80082135;
    func_8004D0F0(var_s0);
    func_8004D0F0(var_s0 + 0xC0);
    func_8004D0F0(var_s0 + 0x80);
    func_800561EC(5);
    func_8004D0F0(var_s0 + 0xA0);
    func_800561EC(6);
    func_8004D0F0(var_s0 + 0x40);
    func_800561EC(3);
    func_8005454C(1, 0x40000000);
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053A20);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053BF4);

void func_80053D90(void) {
    func_80053DB0(0);
}

void func_80053DB0(s32 arg0) {
    if (arg0 == 0) {
        func_8004D0F0(&D_8007E258);
        D_80082134 = 0;
        return;
    }
    D_80082134 = 1;
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053DF4);

void func_80053E48(void) {
    func_8004DC8C();
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053E68);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053F00);

s32 func_80053F60(s32* arg0) {
    s32 temp;

    temp = *arg0 + 1;
    *arg0 = temp;
    if (temp < 0x3D) {
        return 0;
    }
    F3E48C_SetReverbDepth(0x2800);
    return -1;
}

void func_80053FA0(s32 arg0) {
    s32 var_a0;

    if (arg0 == 0) {
        D_800689EC = 0;
        func_80055DFC(0x7F);
        var_a0 = 0x40;
    } else {
        D_800689EC = 1;
        func_80055DFC(0x28);
        var_a0 = 0;
    }
    func_800517B4(var_a0);
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053FF4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_8005414C);

void func_800542D0(s32 arg0, s32 arg1) {
    GStruct16* temp;
    GStruct16From4* mid;

    temp = func_800509F4();
    if (temp != NULL) {
        temp->field_2 = 7;
        mid = (GStruct16From4*)&temp->field_4;
        mid->field_4 = func_80053F00(arg0);
        mid->field_2 = arg1;
        func_80050A38(temp);
    }
}

void func_80054334(s32 arg0) {
    GStruct16* temp;
    GStruct16From4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = func_800509F4();
        if (temp != NULL) {
            temp->field_2 = 8;
            mid = (GStruct16From4*)&temp->field_4;
            mid->field_4 = func_80053F00(arg0);
            func_80050A38(temp);
        }
    }
}

void func_800543AC(s32 arg0) {
    GStruct16* temp;
    GStruct16From4* mid;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        temp = func_800509F4();
        if (temp != NULL) {
            temp->field_2 = 9;
            mid = (GStruct16From4*)&temp->field_4;
            mid->field_4 = func_80053F00(arg0);
            func_80050A38(temp);
        }
    }
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80054424);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800544B8);

INCLUDE_ASM("main/nonmatchings/43FFC", func_8005454C);

void func_80054608(s8 arg0) {
    func_80055D78(arg0);
}

s32 func_8005462C(void)
{
    return ~func_80055DAC(func_80053F00()) != 0;
}

void func_80054658(void)
{
    GStruct16* temp;

    temp = func_800509F4();
    if (temp != NULL) {
        temp->field_2 = 0xD;
        func_80050A38(temp);
    }
}

void func_8005468C(void)
{
    GStruct16* temp;

    temp = func_800509F4();
    if (temp != NULL) {
        temp->field_2 = 0xE;
        func_80050A38(temp);
    }
}

void func_800546C0(void)
{
    GStruct16* temp;

    temp = func_800509F4();
    if (temp != NULL) {
        temp->field_2 = 0xF;
        func_80050A38(temp);
    }
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_800546F4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_8005488C);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80054938);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80054D58);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80054F1C);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055078);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055678);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800558E8);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800559BC);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055A9C);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055B70);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055C00);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055C8C);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055CE0);

void func_80055D78(s8 arg0) {
    if (arg0 < 0) {
        D_8008274B = -1;
        return;
    }
    D_8008274B = arg0;
    if (arg0 == 0) {
        D_8008274B = 1;
    }
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055DAC);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055DFC);

s8 func_80055EE8(void)
{
    return D_80082748;
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055EF8);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055F70);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056068);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056104);

GStruct31* func_800561C0(s32 arg0) {
    if ((u8)arg0 < 0x10) {
        return &D_80082148[(s8)arg0];
    }
    return NULL;
}

void func_800561EC(s32 arg0) {
    GStruct31* temp_s0;
    GStruct31* base;

    if ((u8)arg0 < 0x10) {
        base = D_80082148;
        temp_s0 = &base[(s8)arg0];
        F3D458_Free(temp_s0->field_0);
        temp_s0->field_8 = -1;
        temp_s0->field_0 = NULL;
    }
}

GStruct43* func_80056240(s32 arg0) {
    s32 voiceIdx;
    GStruct43* ptr;

    voiceIdx = (s8)func_8004E060(D_80068A7C, 2, arg0 & 0xFFFF);
    if (voiceIdx < 0) {
        return NULL;
    }
    ptr = (GStruct43*)D_80082148 + voiceIdx;
    ptr->field_0 = voiceIdx;
    func_8004E560(voiceIdx, (s32)func_80056068, (s32)ptr);
    ptr->field_8 = 1;
    return ptr;
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_800562B4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056308);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800563B4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800564C4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800565B8);

INCLUDE_ASM("main/nonmatchings/43FFC", func_8005664C);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800566A4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056700);
