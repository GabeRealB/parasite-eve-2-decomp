#include "common.h"

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/2E348", func_8003DB48);

void func_8003DE14(s32 arg0, s32 arg1, s32 arg2) {
    if (arg0 < 0) {
        D_80070F68.field_48[1].isbg = 0;
        D_80070F68.field_48[0].isbg = 0;
        return;
    }
    D_80070F68.field_48[1].isbg = 1;
    D_80070F68.field_48[0].isbg = 1;
    D_80070F68.field_48[1].r0 = arg0;
    D_80070F68.field_48[0].r0 = arg0;
    D_80070F68.field_48[1].g0 = arg1;
    D_80070F68.field_48[0].g0 = arg1;
    D_80070F68.field_48[1].b0 = arg2;
    D_80070F68.field_48[0].b0 = arg2;
}

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
