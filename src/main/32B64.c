#include "common.h"

#include "main/game.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/32B64", func_80042364);

INCLUDE_ASM("main/nonmatchings/32B64", func_80042500);

INCLUDE_ASM("main/nonmatchings/32B64", func_80042838);

void func_800429C8(s32 arg0)
{
    GBytes4 sp10;
    u8      temp;

    sp10 = D_80013F18;
    if (D_80072311 == 0) {
        func_800260B0(1);
    } else {
        func_800260B0(0);
    }
    if ((arg0 & 0xFFFF) != 0) {
        D_8007A396 = arg0;
        if (D_80062737 != 0) {
            func_80051460(D_80062737, (u8)D_8007A396);
        } else {
            func_80051460(0, (u8)D_8007A396);
        }
    } else {
        temp       = sp10.data[D_80072168.field_1aa];
        D_8007A396 = temp;
        if ((s8)D_80072168.field_1aa == 3) {
            func_80051888();
        } else {
            func_800518E0();
        }
        if (D_80062737 != 0) {
            func_80051460(D_80062737, (u8)D_8007A396);
        } else {
            func_80051460(0, sp10.data[D_80072312]);
        }
    }
}
