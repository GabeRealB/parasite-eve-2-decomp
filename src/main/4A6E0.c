#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/4A6E0", func_80059EE0);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005A94C);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B3B4);

void func_8005B648(CdlCB arg0)
{
    volatile GStruct32* p;

    p = &D_800828F0;
    if (p->field_1 == 0) {
        p->field_4 = CdReadyCallback(arg0);
    } else {
        CdReadyCallback(arg0);
    }
    D_800828F0.field_1 = 1;
}

void func_8005B6A8(void)
{
    volatile GStruct32* p;

    p = &D_800828F0;
    if (p->field_1 != 0) {
        CdReadyCallback(0);
        p->field_4 = 0;
        p->field_1 = 0;
    }
}

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B6EC);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B78C);

void func_8005B830(void)
{
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] | 8;
}

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B84C);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005B920);

void func_8005B968(u32 *arg0)
{
    u32 temp_v1;

    temp_v1 = *arg0;
    if ((temp_v1 >> 1) & 1) {
        *arg0 = temp_v1 & ~8;
        return;
    }
    *arg0 = temp_v1 & ~8;
    D_80082818.field_4C = 0;
    switch ((*arg0 >> 5) & 0xFF) {
    case 6:
    case 7:
        func_8005B6A8();
        goto shared_flush;
    case 8:
        func_8005B6A8();
        CdFlush();
        CdControlF(CdlPause, NULL);
        *arg0 = ((*arg0 | 8) & ~0x1FE0) | 0x1C0;
        break;
    case 9:
    case 11:
    case 12:
    case 14:
        CdFlush();
        *arg0 = (*arg0 & ~0x1FE0) | 0x1A0;
        /* fallthrough */
    case 13:
        if (SpuIsTransferCompleted(0) == 0) {
            *arg0 |= 8;
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 10:
    shared_flush:
        CdFlush();
        break;
    }
}

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BA8C);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BAEC);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BB4C);

void func_8005BB9C(void)
{
    D_80082818.field_6 = 0;
}

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005BBB0);

void func_8005BBF4(void)
{
    D_80082818.field_53 = D_80082818.field_53 & 0xFE;
    D_80082818.unknown_0[1] = D_80082818.unknown_0[1] | 2;
    D_80082818.field_54 = 0;
}

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
