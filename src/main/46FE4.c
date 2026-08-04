#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/46FE4", func_800567E4);

INCLUDE_ASM("main/nonmatchings/46FE4", func_800569D4);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80056B28);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80056E38);

INCLUDE_ASM("main/nonmatchings/46FE4", func_800572FC);

void func_800574BC(void)
{
    u32  i;
    s32* p;

    p = (s32*)&D_80082798;
    i = 0;
    do {
        i++;
        *p = 0;
    } while (i < 2U);

    p = (s32*)&D_800827A0;
    i = 0;
    do {
        i++;
        *p = 0;
    } while (i < 0x11U);

    D_8008277C         = 0;
    D_80082750         = 0;
    D_800827A0.field_8 = 0x51010;
    func_8004E5A0(3, 0x16, 2);
    func_8005B6EC();
    func_800260B0(1);
}

u8 func_80057554(void)
{
    return D_800827A0.field_0;
}

void func_80057564(void)
{
    if ((D_800827A0.field_4 != 0) && (D_800827A0.field_0 != 0)) {
        D_800827A0.field_0 = D_80068B34[D_800827A0.field_0 & 7]();
        func_80059348();
    }
}

s32 func_800575D8(s32 arg0)
{
    volatile GStruct44* p;

    D_800827E4         = 0;
    D_80082754         = 0;
    p                  = &D_80082780;
    p->field_C         = 0;
    p->field_8         = 0;
    p->field_9         = 0;
    D_800827A0.field_4 = arg0;
    D_80082750         = 0;
    return 0;
}

s32 func_80057618(void)
{
    u8                  mode;
    s32                 mem;
    s32                 buf;
    volatile GStruct56* p;

    D_80082798.field_3 = 5;
    p                  = (volatile GStruct56*)&D_800827A0;
    CdIntToPos(p->field_4, (CdlLOC*)&p->field_10);
    buf = D_80082750;
    if (buf != 0) {
        F3D458_Free((void*)buf);
    }
    mem                = (s32)F3D458_Malloc(0x800);
    D_80082750         = mem;
    D_80082778         = mem + 4;
    D_800827A0.field_0 = 5;
    mode               = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, &mode, NULL);
    return 0;
}

s32 func_800576BC(s32 arg0)
{
    s32 temp_s0;

    temp_s0 = arg0 & 0xFF;
    if (temp_s0 != 0) {
        func_80057A88(D_800827A0.field_4 + func_80057A1C((arg0 - 1) & 0xFF));
    }
    return temp_s0;
}

s32 func_80057704(void)
{
    return func_80057ACC();
}

s32 func_80057724(void)
{
    GStruct52* temp;
    s32        ret;

    if (D_80082798.field_0 != 3) {
        return -1;
    }
    if (D_80082798.field_2 != 0) {
        ret = 1;
    } else {
        temp = D_80082794 + D_80082758.field_2;
        func_80057A1C((temp[1].field_3 - temp->field_3 - 1) & 0xFF);
        func_80057B24(0x20);
        ret = 0;
    }
    return ret;
}

s32 func_800577AC(s32 arg0, s32 arg1)
{
    if (func_8005BB4C() != 0) {
        return -1;
    }
    func_80057824(arg0);
    D_800827A0.field_2 = D_80068A80[arg1 & 0xFF] << 7;
    func_800542D0(0x80000000, 0);
    return func_80057894(arg0);
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057824);

s32 func_80057894(s32 arg0)
{
    if (arg0 != 0) {
        func_80057A88(arg0);
    }
    return arg0;
}

s32 func_800578C4(void)
{
    return func_80057ACC();
}

s32 func_800578E4(s32 arg0)
{
    if (arg0 == 0) {
        return -1;
    }
    func_80057B88(arg0, D_80082124);
    return 0;
}

void func_8005791C(s32 arg0)
{
    D_800827A0.field_4 = arg0;
}

void func_80057930(s8 arg0, s32* arg1)
{
    GStruct48 sp10;
    s32*      dest;
    u32       i;

    func_8004E560(arg0, 0, 0);
    func_8004E5C4(arg0, &sp10);
    dest = sp10.field_4;
    i    = 0;
    do {
        *dest = *arg1;
        arg1++;
        i++;
        dest++;
    } while (i < 0x10U);
}

void func_800579A0(s8* arg0, s8* arg1)
{
    *arg0 = func_8004E060(D_80068B28, 3, 0xFFFF);
    *arg1 = func_8004E060(D_80068B28, 3, 0xFFFF);
    F3E48C_DisableVoice(*arg0);
    F3E48C_DisableVoice(*arg1);
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057A1C);

s32 func_80057A88(s32 arg0)
{
    s32 temp_v0;

    temp_v0 = D_80082798.field_1;
    if ((temp_v0 == 4) || (temp_v0 == 0)) {
        D_800827A0.field_C = arg0;
        D_800827A0.field_0 = 1;
        D_80082798.field_0 = 4;
    }
    return 0;
}

s32 func_80057ACC(void)
{
    volatile GStruct4* p;
    s32                ret;

    p = &D_80082798;
    if (p->field_0 != 3) {
        ret        = -1;
        p->field_1 = 4;
        p->field_2 = 1;
    } else {
        ret                = 0;
        p->field_2         = 0;
        p->field_1         = 1;
        D_800827A0.field_0 = 2;
    }
    return ret;
}

void func_80057B24(s32 arg0)
{
    GStruct55*          p;
    volatile GStruct56* parent;

    p      = &D_800827B4;
    parent = (volatile GStruct56*)p;
    parent = parent - 1;
    func_8004D200(p, (parent->field_2 >> 7) & 0xFF, 0, arg0);
    D_80082798.field_1 = 4;
    D_80082798.field_2 = 1;
    parent->field_0    = 3;
}

void func_80057B88(s32 arg0, s32 arg1)
{
    D_800827A0.field_4  = arg0;
    D_80082758.field_10 = arg1;
    D_80082798.field_4  = 1;
    D_800827A0.field_0  = 6;
}

s32 func_80057BC0(void)
{
    return 0;
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057BC8);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057C74);

void func_80057D24(void)
{
    D_800827A0.field_1 = 1;
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057D3C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057E1C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80057FAC);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80058320);

INCLUDE_ASM("main/nonmatchings/46FE4", func_8005842C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_8005854C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80058748);

void func_800588D8(void)
{
    volatile GStruct19* p;
    u8                  temp;

    if (D_80068B5C != 0) {
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        D_80068B5C = 0;
    }
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xF7;
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] & 0xBF;
    func_800B0118(0, 0);
    p                       = &D_80082818;
    temp                    = p->unknown_0[2];
    p->unknown_0[2]         = temp & 0xF7;
    D_80082808              = 0;
    D_80082818.unknown_0[0] = D_80082818.unknown_0[0] | 1;
}

INCLUDE_ASM("main/nonmatchings/46FE4", func_8005896C);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80058ED4);

INCLUDE_ASM("main/nonmatchings/46FE4", func_80059348);
