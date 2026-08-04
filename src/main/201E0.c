#include "common.h"

#include <psyq/rand.h>

#include "main/game.h"
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

INCLUDE_ASM("main/nonmatchings/201E0", func_800301FC);

INCLUDE_ASM("main/nonmatchings/201E0", func_800303AC);
