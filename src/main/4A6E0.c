#include "common.h"

#include <psyq/libapi.h>
#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/4A6E0", func_80059EE0);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005A94C);

s32 func_8005B3B4(u32* arg0)
{
    struct {
        u8     result[8];
        s8     mode;
        u8     pad[7];
        CdlLOC loc;
    } sp;
    s32          sync;
    u32          flags;
    u32          temp;
    register u32 a asm("v0");
    register u32 b asm("v1");

    flags = *arg0;
    if ((flags >> 1) & 1) {
        temp  = flags & ~2;
        temp  = temp & ~0xFF0;
        *arg0 = temp | 0x10;
    }

    a          = *(volatile u32*)arg0;
    b          = *(volatile u32*)arg0;
    a          = (a >> 4) & 0xFF;
    b          = (b >> 4) & 0xFF;
    D_80068B66 = a;
    switch (b) {
        case 1:
            if (CdControlB(CdlNop, NULL, sp.result) == 0) {
                return 0;
            }
            if (sp.result[0] & CdlStatShellOpen) {
                return 0;
            }
            if (sp.result[0] & CdlStatStandby) {
                *arg0 = (*arg0 & ~0xFF0) | 0x20;
                case 2:
                    if (CdControl(CdlGetTN, NULL, sp.result) != 0) {
                        *arg0 = (*arg0 & ~0xFF0) | 0x40;
                        case 3:
                            sync = CdSync(1, sp.result);
                            if (sync == CdlDiskError) {
                                *arg0 = (*arg0 & ~0xFF0) | 0x20;
                            } else if (sync == CdlComplete) {
                                *arg0 = (*arg0 & ~0xFF0) | 0x40;
                                case 4:
                                    CdIntToPos(0, &sp.loc);
                                    if (CdControl(CdlSeekL, (u8*)&sp.loc, sp.result) != 0) {
                                        *arg0 = (*arg0 & ~0xFF0) | 0x50;
                                        case 5:
                                            sync = CdSync(1, sp.result);
                                            if ((sync == CdlDiskError) && (sp.result[0] & CdlStatError) &&
                                                (sp.result[1] & 0x40)) {
                                                *arg0 = (*arg0 & ~0xFF0) | 0x10;
                                            } else if (sync == CdlComplete) {
                                                *arg0 = (*arg0 & ~0xFF0) | 0x60;
                                                case 6:
                                                    sp.mode = -0x60;
                                                    if (CdControl(CdlSetmode, (u8*)&sp.mode, NULL) != 0) {
                                                        *arg0               = (*arg0 & ~0xFF0) | 0x70;
                                                        D_80082818.field_56 = 0;
                                                    }
                                            }
                                    }
                            }
                    }
            }
            break;
        case 7:
            D_80082818.field_56 = D_80082818.field_56 + 1;
            if (D_80082818.field_56 >= 4) {
                return 1;
            }
            break;
    }
    return 0;
}

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

void func_8005B6EC(void)
{
    s32* ptr;
    u32  i;

    ptr = (s32*)&D_800828F0;
    for (i = 0; i < 0x16; i++) {
        *ptr++ = 0;
    }

    ptr = (s32*)&D_80082818;
    for (i = 0; i < 0x36; i++) {
        *ptr++ = 0;
    }

    SetRCnt(RCntCNT2, 0xFFFF, RCntMdNOINTR);
    StartRCnt(RCntCNT2);
    D_80068B58 = 0;
    D_80082808 = 0;
    D_80082810 = D_80082808;
    D_80068B6A = 0;
    D_80068B5C = 0;
}

void func_8005B78C(void)
{
    volatile GStruct19* p;

    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xFD;
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xF7;
    p                       = &D_80082818;
    p->unknown_0[4]         = 1;
    p->field_18             = 0;
    SpuSetIRQ(0);
    SpuSetIRQCallback(func_8005B830);
    SpuSetIRQAddr((p->field_3C + p->field_42 + 0x4F) & ~0x3F);
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xBF;
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] | 1;
}

void func_8005B830(void)
{
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] | 8;
}

void func_8005B84C(s16 arg0)
{
    GStruct74*               p;
    volatile GStruct19*      q;
    GStruct74Entry*          ch1b;
    register GStruct74Entry* ch1 asm("v0");
    s16                      val;
    s32                      t0;
    s32                      t1;

    p = &D_80082870;
    q = (volatile GStruct19*)p - 1;

    if ((q->unknown_0[0] >> 1) & 1) {
        if (q->unknown_0[1] & 1) {
            t0               = p->ch[0].field_4;
            t1               = p->ch[1].field_4;
            p->ch[0].field_4 = t0 | 3;
            p->ch[1].field_4 = t1 | 3;
        } else {
            p->ch[1].field_4 = 3;
            p->ch[0].field_4 = 3;
            q->unknown_0[1]  = q->unknown_0[1] | 1;
        }
    }

    if (D_80082818.field_53 & 2) {
        ch1b             = &p->ch[1];
        val              = (s16)((arg0 * 0xB5) >> 8);
        ch1b->field_8    = val;
        p->ch[0].field_A = val;
        ch1b->field_A    = val;
        p->ch[0].field_8 = val;
        return;
    }
    ch1              = &p->ch[1];
    ch1->field_A     = arg0;
    p->ch[0].field_8 = arg0;
    ch1->field_8     = 0;
    p->ch[0].field_A = 0;
}

void func_8005B920(s32 arg0)
{
    volatile GStruct19* p;
    u8                  temp;

    p    = &D_80082818;
    temp = p->unknown_0[1];
    if (temp >> 7) {
        p->field_14     = arg0;
        p->unknown_0[1] = p->unknown_0[1] | 8;
        p->unknown_0[0] = p->unknown_0[0] | 1;
    }
}

void func_8005B968(u32* arg0)
{
    u32 temp_v1;

    temp_v1 = *arg0;
    if ((temp_v1 >> 1) & 1) {
        *arg0 = temp_v1 & ~8;
        return;
    }
    *arg0               = temp_v1 & ~8;
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

void func_8005BA8C(u32* arg0)
{
    func_8005B968(arg0);
    if (!((*arg0 >> 3) & 1) && (D_80082818.field_8 != NULL)) {
        D_80082818.field_8(0);
    }
}

void func_8005BAEC(s16 arg0)
{
    u8              temp;
    s16             idx;
    u32             flags;
    GStruct32Entry* entry;

    temp = D_800828F0.field_0;
    if (arg0 != 0) {
        idx   = arg0 - 1;
        entry = (GStruct32Entry*)&D_800828F0.entries[idx];
        flags = entry->field_0;
        if (flags & 1) {
            entry->field_0 = (flags & ~1) | 4;
        }
        D_800828F0.field_0 = temp;
    }
}

s32 func_8005BB4C(void)
{
    if (D_80082818.unknown_0[0] & 1) {
        return 1;
    }
    return (D_800828F0.field_3 != D_800828F0.field_2) ? 1 : (D_80082818.field_54 != 0);
}

void func_8005BB9C(void)
{
    D_80082818.field_6 = 0;
}

void func_8005BBB0(s32 arg0)
{
    if ((s8)arg0) {
        D_80082818.field_53 = D_80082818.field_53 | 2;
    } else {
        D_80082818.field_53 = D_80082818.field_53 & 0xFD;
    }
}

void func_8005BBF4(void)
{
    D_80082818.field_53     = D_80082818.field_53 & 0xFE;
    D_80082818.unknown_0[1] = D_80082818.unknown_0[1] | 2;
    D_80082818.field_54     = 0;
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
