#include "common.h"

#include "main/game.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/33300", func_80042B00);

INCLUDE_ASM("main/nonmatchings/33300", func_80042DF8);

INCLUDE_ASM("main/nonmatchings/33300", func_80042F54);

INCLUDE_ASM("main/nonmatchings/33300", func_80043028);

INCLUDE_ASM("main/nonmatchings/33300", func_800430E4);

INCLUDE_ASM("main/nonmatchings/33300", func_80043198);

void func_800431FC(GStruct0* arg0)
{
    if (func_8001D344() != 0) {
        D_80062734 = 0xFF;
        func_8002CCB8(arg0);
    }
}

INCLUDE_ASM("main/nonmatchings/33300", func_8004323C);

INCLUDE_ASM("main/nonmatchings/33300", func_80043310);

INCLUDE_ASM("main/nonmatchings/33300", func_800435F8);

void func_80043718(s16 arg0, s16 arg1, s16 arg2, s32 arg3)
{
    DR_TPAGE *p;

    p = (DR_TPAGE *)D_80070EE0;
    D_80070EE0 = (u8 *)(p + 1);
    SetDrawTPage(p, 1, 0, GetTPage(0, arg0, arg1, arg2) & 0xFFFF);
    AddPrim(D_800710A0 + arg3, p);
}

INCLUDE_ASM("main/nonmatchings/33300", func_8004379C);

INCLUDE_ASM("main/nonmatchings/33300", func_80043854);
