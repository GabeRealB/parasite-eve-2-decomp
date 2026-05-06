#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/libpress.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/4CF8", func_800144F8);

INCLUDE_ASM("main/nonmatchings/4CF8", func_80014650);

void func_800148A0(void)
{
    func_80056700();
    while (D_80082798.field_2 != 4) {
    }
}

void func_800148EC(void)
{
    func_800574BC();
}

INCLUDE_ASM("main/nonmatchings/4CF8", func_8001490C);

INCLUDE_ASM("main/nonmatchings/4CF8", func_800149E8);

void func_80014A50(void)
{
    u8 param[8];

    CdInit();
    param[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, param, NULL);
    func_800574BC();
    func_8001D3F8();
}

void func_80014A98(s32 mode)
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
    func_8001D3F8();
}

INCLUDE_ASM("main/nonmatchings/4CF8", func_80014B38);

INCLUDE_ASM("main/nonmatchings/4CF8", func_80014C2C);

INCLUDE_ASM("main/nonmatchings/4CF8", func_80014C4C);
