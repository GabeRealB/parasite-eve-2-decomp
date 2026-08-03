#include "common.h"

#include <psyq/libetc.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

void func_800271D4(void)
{
    s32 flag; // The indirection is required.

    SetDispMask(0);
    F04CF8_800148A0();
    ResetCallback();
    F04CF8_80014A98(1);
    VSync(10);

    GameResetScratchHead();
    D_8005EC64++;
    func_800144F8(0, 0);
    Mem_Init();
    func_8002D428();
    func_80028718();
    func_800281D4();

    Mem_Set(&D_80070F68, 0, sizeof(D_80070F68));
    D_80070F68.field_120 = 1;
    D_80070F68.field_124 = 0;
    D_80070F68.field_101 = 0;
    D_80070F68.field_1d  = -1;
    D_80070F68.field_1e  = 0;
    D_80070F68.field_10d = 0;
    D_80070F68.field_0   = 0;
    D_80070F68.field_4   = 0;
    D_80070F68.field_8   = 0;
    D_80070F68.field_c   = 0;
    D_80070F68.field_10  = 0;
    D_80070F68.field_14  = 0;
    func_800280F4(0);

    D_8005EC70 = 0;
    F179D4_ClearOTag(0);
    F179D4_ClearOTag(1);
    func_8004CFE8();
    func_80053FA0(0);
    F04CF8_800148EC();
    VSyncCallback(func_80027498);

    flag                = 1;
    D_80070F68.field_1f = flag;
    func_8003DB48(0x1010);
    Mem_Set(D_800626A8, 0, 0x1C);
}

INCLUDE_ASM("main/nonmatchings/179D4", func_8002731C);

INCLUDE_ASM("main/nonmatchings/179D4", func_80027498);

INCLUDE_ASM("main/nonmatchings/179D4", func_8002764C);

INCLUDE_ASM("main/nonmatchings/179D4", func_8002785C);

INCLUDE_ASM("main/nonmatchings/179D4", func_80027E7C);

INCLUDE_ASM("main/nonmatchings/179D4", func_80027F48);

INCLUDE_ASM("main/nonmatchings/179D4", func_800280F4);

void F179D4_ClearOTag(s16 tableIdx)
{
    u_long* tableStart = D5F414_OrderingTables + tableIdx * C5F414_OTAG_ENTRIES;
    ClearOTagR(tableStart, C5F414_OTAG_ENTRIES);
    *tableStart = C5F414_OTAG_END_PRIM;
}

void func_800281D4(void)
{
    RECT rect;
    GStruct35* otCtx;
    u_long* ot;
    s32 depth;

    if (D_8005EC64 == 1) {
        ResetGraph(0);
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0x200;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    InitGeom();

    otCtx = D_80070EE8;
    depth = 0xA;
    otCtx->field_0 = depth;
    ot = D5F414_OrderingTables;
    otCtx->field_10 = ot + C5F414_OTAG_ENTRIES - 1;
    otCtx->field_4 = ot;
    otCtx[1].field_0 = depth;
    otCtx[1].field_4 = ot + C5F414_OTAG_ENTRIES;
    otCtx[1].field_10 = ot + 2 * C5F414_OTAG_ENTRIES - 1;
    func_80028290();
    func_80027E7C();
    func_8003AF04();
    D_80070F68.field_100 = 0;
}

void func_80028290(void)
{
    if (D_8005EC64 == 1) {
        func_8002CFDC(0, 0x1F, 0, 0);
    } else {
        func_8002CFDC(0, 0x20, 0, 0);
    }
}

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
    F04CF8_80014A50();
    Mem_Set(&D_800710A8, 0, sizeof(D_800710A8));
    D_8005EC64 = 0;
    func_800271D4();
    func_8002785C();
}

u32 func_80028404(void)
{
    return D_8005EC64;
}
