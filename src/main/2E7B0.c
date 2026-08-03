#include "common.h"

#include "main/game.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003DFB0);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E210);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E324);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E438);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E4BC);

void func_8003E540(void) {
    func_8003E814();
}

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E560);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E610);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E64C);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E698);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E6E4);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E72C);

void func_8003E814(void) {
    GStruct14* temp;

    temp = D4F564_8005ED64;
    func_800144F8(temp->field_7, temp->field_6);
    D_80070F68.field_1e = 0;
    D_80070F68.field_10d = 0;
}

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E854);

INCLUDE_ASM("main/nonmatchings/2E7B0", func_8003E904);

void func_8003E9A4(void) {
    D_8007A0E4 = 0x10000;
    D_8007A0E0 = (void*)D_80068F88;
}

void func_8003E9C4(void) {
    D_8007A0E0 = D_800740E0;
    D_8007A0E4 = 0x6000;
}
