#include "common.h"
#include "main/boot.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/task.h"

#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/libpress.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/boot", func_800144F8);

void Boot_LoadInitialFile(Task* task)
{
    u8             modeParam[8];
    u8             param1[8];
    u8             param2[8];
    s32            state;
    s32            next;
    u8             fade;
    register Task* a0 asm("s0");
    CdCmdQueue*    queue;
    register s32   ch asm("a1");
    register u32   size asm("a2");

    a0    = task;
    queue = &CdCmd_Queue;
    state = a0->field_30;
    switch (state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
    }
    return;

L_case0:
    func_8003DB48(0xD010);
    modeParam[0] = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, modeParam, NULL);
    SetDispMask(0);
    Fs_ScanIsoDirectory(1);
    Display_State.field_100 = 1;
    CdCmd_Enqueue(0x55, NULL, NULL);
    func_800144F8(0, 0);
    while (queue->field_1FE != 0xFF) {
        func_80040820();
    }
    param1[3] = 0;
    param1[2] = 0;
    param1[0] = 1;
    param2[0] = 0;
    param2[1] = 0;
    param2[2] = 0;
    param2[3] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
    a0->field_2a = 0xFF;
    fade         = a0->field_2a;
    func_8002BA9C(fade, fade, fade, 2);
    goto advance;

L_case1:
    SetDispMask(1);
    a0->field_2a -= 8;
    if (a0->field_2a <= 0) {
        a0->field_2a = 0;
        a0->field_30 = a0->field_30 + 1;
    }
    goto do_fade;

L_case2:
    if (a0->field_2a < 0x5A) {
        a0->field_2a = a0->field_2a + 1;
    }
    if (func_8001D344() == 0) {
        return;
    }
    if (a0->field_2a < 0x5A) {
        return;
    }
    next         = a0->field_30;
    a0->field_2a = 0;
    goto advance_inc;

L_case3:
    a0->field_2a += 8;
    if (a0->field_2a < 0x100) {
        goto do_fade;
    }
    ch   = 0;
    size = 0x20000;
    asm("" : "+r"(ch), "+r"(size));
    Mem_Set(D4CB64_ImgBuffers, ch, size | 0x5800);
advance:
    next = a0->field_30;
advance_inc:
    a0->field_30 = next + 1;
    return;

do_fade:
    fade = a0->field_2a;
    func_8002BA9C(fade, fade, fade, 2);
    return;

L_case4:
    Task_Spawn(0, 0xD, 0, 0);
    Task_Kill(a0);
    SetDispMask(1);
    Display_State.field_112 = 0;
}

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

INCLUDE_ASM("main/nonmatchings/boot", func_8001490C);

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

void func_80014B38(Task* arg0)
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
                Task_SpawnFromTable((TaskDesc*)&D_80094C8C, 0, 0, 0);
                Task_Kill(arg0);
                Display_State.field_112 = 0;
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
