#include "common.h"

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

INCLUDE_ASM("main/nonmatchings/201E0", func_80030074);

INCLUDE_ASM("main/nonmatchings/201E0", func_800300EC);

INCLUDE_ASM("main/nonmatchings/201E0", func_800301FC);

INCLUDE_ASM("main/nonmatchings/201E0", func_800303AC);
