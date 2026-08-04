#include "common.h"

#include <psyq/rand.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/201E0", func_8002F9E0);

INCLUDE_ASM("main/nonmatchings/201E0", func_8002FB84);

s32 func_8002FCBC(u8* arg0)
{
    GStruct38 sp10;

    sp10.field_C = 4;
    sp10.field_0 = 0;
    sp10.field_2 = 0;
    sp10.field_4 = 0;
    sp10.field_8 = 0;
    sp10.field_D = 2;
    sp10.field_E = 0;
    func_8002EDFC(&sp10, arg0);
    return -sp10.field_0;
}

INCLUDE_ASM("main/nonmatchings/201E0", func_8002FD08);

INCLUDE_ASM("main/nonmatchings/201E0", func_8002FDCC);

void func_8002FEAC(void* arg0, void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6)
{
    func_8002FDCC(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
}

INCLUDE_ASM("main/nonmatchings/201E0", func_8002FEE0);

void func_80030074(void)
{
    RECT rect;

    rect.x = 0x100;
    rect.y = 0xF3;
    rect.w = 0x40;
    rect.h = 1;
    LoadImage(&rect, D_80060910);

    rect.x = 0x3D0;
    rect.y = 0x1FF;
    rect.w = 0x30;
    rect.h = 1;
    LoadImage(&rect, D_800609B0);
}

void func_800300EC(u8* arg0, s32 arg1)
{
    s32 i;

    i = 0;
    do {
        *arg0 = D_80060DC8[i];
        i++;
        arg0++;
    } while (i < 0xC);

    *arg0   = D_80060E08[arg1];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    arg0[1] = 0;
}

void func_800301FC(void)
{
    u8(*a)[0x6C];
    u8(*b)[0xB0];
    u8(*c)[0x24];
    u8(*d)[0xE4];
    u8(*e)[0xA4];
    McSaveData* p;
    s32         one;
    s32         two;
    s32         idx;

    Mem_Set(&D_80073B88, 0, 0x40);
    Mem_Set(D_80073B88.field_40, 0xFF, 0x40);
    Mem_Set(D_80073980, 0, 0x100);
    Mem_Set(&D_80073980[0x100], 0xFF, 0x100);

    a = D_800733F0;
    Mem_Set(a, 0, 0x6C);
    do {
        b = D_800734C8;
        Mem_Set(b, 0, 0xB0);
        c = D_80073628;
        Mem_Set(c, 0, 0x24);
        d = D_80073670;
        Mem_Set(d, 0, 0xE4);
        e = D_80073838;
        Mem_Set(e, 0, 0xA4);
        Mem_Set(a + 1, 0xFF, 0x6C);
        Mem_Set(b + 1, 0xFF, 0xB0);
        Mem_Set(c + 1, 0xFF, 0x24);
        Mem_Set(d + 1, 0xFF, 0xE4);
        Mem_Set(e + 1, 0xFF, 0xA4);
        p = &Mc_SaveData;
    } while (0);

    one          = 1;
    p->field_6   = 0x14;
    two          = 2;
    p->field_7   = one;
    p->field_4   = one;
    p->field_5   = one;
    p->field_8   = 7;
    p->field_9   = one;
    p->field_5C5 = two;
    p->field_22  = one;
    func_8004C4D0();
    idx                         = p->field_22 - 1;
    (&D_80073B88)[idx].field_21 = two;
}

INCLUDE_ASM("main/nonmatchings/201E0", func_800303AC);
