#include "common.h"

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/3B458", func_8004AC58);

s32 func_8004ACAC(s32 arg0)
{
    s32 idx;

    idx = arg0 / 2;
    if (arg0 & 1) {
        return D_80073980[idx + 4] & 0xF;
    }
    return D_80073980[idx + 4] >> 4;
}

void func_8004ACF0(void)
{
    func_8002C868(0, 1, 0x800);
}
