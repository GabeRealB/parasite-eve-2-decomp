#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/libpress.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/4CF8", func_800144F8);

INCLUDE_ASM("main/nonmatchings/4CF8", Boot_LoadInitialFile);

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

void func_800149E8(s32 arg0, s32 arg1, s32 arg2)
{
    RECT              rect;
    F04CF8_ImageSlot* entries;

    entries = D_8005C37C[arg0];
    if (arg2 == 0) {
        rect.y = 0;
    } else {
        rect.y = 0x110;
    }
    rect.w = 0x140;
    rect.h = 0xF0;
    rect.x = 0;
    LoadImage(&rect, entries[arg1].field_0);
}

void F04CF8_80014A50(void)
{
    u8 param[8];

    CdInit();
    param[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, param, NULL);
    func_800574BC();
    CdCmd_ClearQueue();
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
    CdCmd_ClearQueue();
}

void func_80014B38(GStruct0* arg0)
{
    u8  modeParam[8];
    u8  param1[8];
    u8  param2[8];
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
            CdCmd_Enqueue(0x21, param1, param2);
            arg0->field_30 = arg0->field_30 + 1;
            return;
        case 1:
            if (func_8001D344() != 0) {
                SetDispMask(1);
                func_800144F8(0, 0);
                func_8002CFA0((TaskDesc*)&D_80094C8C, 0, 0, 0);
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

bool Fs_StageCdfIsAvailable(u32 stageIdx)
{
    return Fs_StageCdfSectors[(u8)stageIdx] != 0;
}
