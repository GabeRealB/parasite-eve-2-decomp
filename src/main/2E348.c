#include "common.h"

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/2E348", func_8003DB48);

INCLUDE_ASM("main/nonmatchings/2E348", func_8003DE14);

void func_8003DE58(void) {
    func_8003DB48(0x1010);
}

void func_8003DE78(s8 arg0) {
    s8 val;

    val = arg0;
    if (arg0 >= 8) {
        val = 8;
    } else if (arg0 < -7) {
        val = -8;
    }
    D_80070F68.field_126 = val;
}
