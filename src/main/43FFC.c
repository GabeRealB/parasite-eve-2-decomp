#include "common.h"

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/43FFC", func_800537FC);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053A20);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053BF4);

void func_80053D90(void) {
    func_80053DB0(0);
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053DB0);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053DF4);

void func_80053E48(void) {
    func_8004DC8C();
}

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053E68);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053F00);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80053F60);

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

INCLUDE_ASM("main/nonmatchings/43FFC", func_800542D0);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80054334);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800543AC);

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

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055D78);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055DAC);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055DFC);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055EE8);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055EF8);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80055F70);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056068);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056104);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800561C0);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800561EC);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056240);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800562B4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056308);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800563B4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800564C4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800565B8);

INCLUDE_ASM("main/nonmatchings/43FFC", func_8005664C);

INCLUDE_ASM("main/nonmatchings/43FFC", func_800566A4);

INCLUDE_ASM("main/nonmatchings/43FFC", func_80056700);
