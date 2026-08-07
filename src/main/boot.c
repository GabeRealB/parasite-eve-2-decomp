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

void func_800144F8(s32 arg0, s32 arg1)
{
    F04CF8_ImageSlot* entries;
    F04CF8_ImageSlot* slot;
    s32               i;
    u32               t;
    size_t*           p88;
    size_t*           p90;
    int*              p98;
    size_t            temp;

    entries = D_8005C37C[arg0];
    if ((Display_State.field_12a == 0) || (arg0 == 0)) {
        D_800691F4         = (u8*)0x80179950;
        D_800691F8         = 0x836B0;
        D_80068F88         = 0x80179950;
        GActiveAuxHeap     = (u8*)0x80189950;
        GActiveAuxHeapSize = 0x4D6B0;
    } else {
        t                  = arg1 * 8;
        t                 += (u32)entries;
        slot               = (F04CF8_ImageSlot*)t;
        D_800691F4         = (u8*)slot->field_0;
        D_800691F8         = slot->field_4 + 0x26000;
        D_80068F88         = (size_t)slot->field_0;
        GActiveAuxHeap     = (u8*)(D_80068F88 + 0x10000);
        GActiveAuxHeapSize = slot->field_4 - 0x10000;
    }
    i            = 0;
    D_80068F90   = 0x10000;
    GAuxHeap     = GActiveAuxHeap;
    GAuxHeapSize = D_800691F8 - 0x10000;
    do {
        *(u8*)((D_80068F90 - (i & 0xFF)) + D_80068F88 - 1) = 0;
        i                                                 += 1;
    } while ((u32)(i & 0xFF) < 0xAU);
    p98  = &D_80068F98;
    p88  = &D_80068F88;
    p90  = &D_80068F90;
    temp = *p90 - 0xA;
    *p98 = *p88 + temp;
}

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
    if (CdCmd_IsIdle() == 0) {
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
    CdAudio_Begin();
    while (CdAudio_Phase.field_2 != 4) {
    }
}

void F04CF8_800148EC(void)
{
    CdAudio_Init();
}

void func_8001490C(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    RECT              rect;
    F04CF8_ImageSlot* entries;
    u8*               ptr;
    size_t            size;
    size_t            imgBufSize;
    size_t*           pSize;

    entries = D_8005C37C[arg0];
    rect.x  = 0;
    if (arg2 != 0) {
        rect.y = 0;
    } else {
        rect.y = 0x110;
    }
    rect.w = 0x140;
    rect.h = 0xF0;
    StoreImage(&rect, entries[arg1].field_0);
    DrawSync(0);

    imgBufSize = 0x25800;
    do {
        D_80068F90 = 0x10000;
    } while (0);
    pSize        = &GActiveAuxHeapSize;
    size         = 0x10000 - arg3;
    *pSize       = size;
    D_800691F8   = 0x10000;
    GAuxHeapSize = size;

    ptr            = (u8*)entries[arg1].field_0 + imgBufSize;
    D_80068F88     = (size_t)ptr;
    GActiveAuxHeap = ptr + arg3;
    D_800691F4     = ptr;
    GAuxHeap       = ptr + arg3;
}

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
    CdAudio_Init();
    CdCmd_ClearQueue();
}

void F04CF8_80014A98(s32 mode)
{
    u8 ctrlParam[8];

    CdFlush();
    VSync(3);
    CdControlB(CdlPause, NULL, NULL);
    if (Wip_SysFlags.field_6 != 0) {
        DecDCTReset(0);
        StClearRing();
        StUnSetRing();
        Wip_SysFlags.field_6 = 0;
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
            if (CdCmd_IsIdle() != 0) {
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
    CdCmd_Dispatch();
}

bool Fs_StageCdfIsAvailable(u32 stageIdx)
{
    return Fs_StageCdfSectors[(u8)stageIdx] != 0;
}
