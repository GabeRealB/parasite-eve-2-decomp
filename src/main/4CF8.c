#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/libpress.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/4CF8", func_800144F8);

INCLUDE_ASM("main/nonmatchings/4CF8", func_80014650);

void F04CF8_800148A0(void)
{
    func_80056700();
    while (D_80082798.field_2 != 4) {
    }
}

void F04CF8_800148EC(void)
{
    func_800574BC();
}

INCLUDE_ASM("main/nonmatchings/4CF8", func_8001490C);

INCLUDE_ASM("main/nonmatchings/4CF8", func_800149E8);

void F04CF8_80014A50(void)
{
    u8 param[8];

    CdInit();
    param[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, param, NULL);
    func_800574BC();
    F0C37C_ClearD80068FA0();
}

void F04CF8_80014A98(s32 mode)
{
    u8 ctrlParam[8];

    CdFlush();
    VSync(3);
    CdControlB(CdlPause, NULL, NULL);
    if (D_800710A8.field_6 != 0) {
        DecDCTReset(0);
        StClearRing();
        StUnSetRing();
        D_800710A8.field_6 = 0;
    }
    CdReset(mode);
    ctrlParam[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, ctrlParam, NULL);
    F0C37C_ClearD80068FA0();
}

void func_80014B38(GStruct0* arg0)
{
    u8 modeParam[8];
    u8 param1[8];
    u8 param2[8];
    s32 state;

    state = arg0->field_30;
    switch (state) {
    case 0:
        modeParam[0] = CdlModeSpeed | CdlModeSize1;
        CdControlB(CdlSetmode, modeParam, NULL);
        SetDispMask(0);
        param1[3] = 0;
        param1[2] = 0;
        param1[0] = 1;
        param2[0] = 0;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        func_8001D2B0(0x21, param1, param2);
        arg0->field_30 = arg0->field_30 + 1;
        return;
    case 1:
        if (func_8001D344() != 0) {
            SetDispMask(1);
            func_800144F8(0, 0);
            func_8002CFA0((GStruct2*)&D_80094C8C, 0, 0, 0);
            func_8002CCB8(arg0);
            D_80070F68.field_112 = 0;
        }
        return;
    }
}

void func_80014C2C(void)
{
    func_8001DB84();
}

bool F04CF8_StageCdfIsAvailable(u32 stageIdx)
{
    return D5B498_StageSectors[(u8)stageIdx] != 0;
}
