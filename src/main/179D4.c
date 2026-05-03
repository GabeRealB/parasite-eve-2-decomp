#include "common.h"

#include <psyq/libetc.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/179D4", func_800271D4);

INCLUDE_ASM("main/nonmatchings/179D4", func_8002731C);

INCLUDE_ASM("main/nonmatchings/179D4", func_80027498);

INCLUDE_ASM("main/nonmatchings/179D4", func_8002764C);

INCLUDE_ASM("main/nonmatchings/179D4", func_8002785C);

INCLUDE_ASM("main/nonmatchings/179D4", func_80027E7C);

INCLUDE_ASM("main/nonmatchings/179D4", func_80027F48);

INCLUDE_ASM("main/nonmatchings/179D4", func_800280F4);

INCLUDE_ASM("main/nonmatchings/179D4", func_80028180);

INCLUDE_ASM("main/nonmatchings/179D4", func_800281D4);

INCLUDE_ASM("main/nonmatchings/179D4", func_80028290);

INCLUDE_ASM("main/nonmatchings/179D4", func_800282D8);

// TODO
void GameMain(void)
{
    GameResetScratchHead();
    ResetCallback();
    SetVideoMode(MODE_NTSC);
    func_8004CFC8();
    func_80033BBC();
    func_80028664();
    func_80014A50();
    Mem_Set(&D_800710A8, 0, 0x20);
    D_8005EC64 = 0;
    func_800271D4();
    func_8002785C();
}

INCLUDE_ASM("main/nonmatchings/179D4", func_80028404);
