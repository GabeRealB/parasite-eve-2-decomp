#include "common.h"

#include <psyq/libspu.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/4A6E0", func_80059EE0);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005A94C);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B3B4);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B648);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B6A8);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B6EC);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B78C);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B830);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B84C);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B920);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B968);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BA8C);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BAEC);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BB4C);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BB9C);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BBB0);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BBF4);

s32 func_8005BC28(void)
{
    CdFlush();
    return 0;
}

void func_8005BC48(s32 arg0, u32 arg1)
{
    if (arg0 == 1) {
        if (D_80068B5C != 0) {
            SpuSetIRQ(0);
            SpuSetIRQCallback(0);
        }
        D_80068B5C = arg0;
        SpuSetIRQCallback(func_8005B830);
        SpuSetIRQAddr(arg1);
        SpuSetIRQ(1);
    } else if (D_80068B5C != 0) {
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        D_80068B5C = 0;
    }
}

void func_8005BCF8(void)
{
}
