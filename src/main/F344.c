#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libpress.h>

#include "main/fs.h"
#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/F344", func_8001EB44);

INCLUDE_ASM("main/nonmatchings/F344", func_8001ED20);

INCLUDE_ASM("main/nonmatchings/F344", func_8001EDC8);

INCLUDE_ASM("main/nonmatchings/F344", func_8001EED8);

INCLUDE_ASM("main/nonmatchings/F344", func_8001EF9C);

INCLUDE_ASM("main/nonmatchings/F344", func_8001F180);

s32 func_8001F2FC(s32 arg0)
{
    RECT        rect;
    s32         ac14;
    s32         f12a;
    CdCmdQueue* p;

    if (func_8001E2D4() & 0xFFFF) {
        p = &CdCmd_Queue;
        DecDCToutCallback(0);
        DecDCTReset(0);
        StClearRing();
        StUnSetRing();
        ac14               = D_8006AC14;
        D_800710A8.field_6 = 0;
        p->field_24A       = 0;
        p->field_1FA       = 0;
        p->field_1F4       = 0;
        p->field_1E2       = 0;
        p->field_1E4       = 0;
        if (ac14 != 0) {
            f12a = D_80070F68.field_12a;
            if (f12a == 1) {
                if (arg0 & 0xFFFF) {
                    rect.y = 0;
                    rect.x = 0;
                    if (ac14 == f12a) {
                        rect.w = 0x1E0;
                    } else {
                        rect.w = 0x140;
                    }
                    rect.h = 0xF0;
                    ClearImage(&rect, 0, 0, 0);
                    rect.y = 0x110;
                    ClearImage(&rect, 0, 0, 0);
                }
                func_8003DB48(0xD010);
            }
            p->field_1E6         = 0;
            D_80070F68.field_106 = 0;
        } else if (D_8006AC3C != 0) {
            p->field_244 = 0;
        }
        return 1;
    }
    return 0;
}

INCLUDE_ASM("main/nonmatchings/F344", func_8001F430);

INCLUDE_ASM("main/nonmatchings/F344", func_8001F6B8);

INCLUDE_ASM("main/nonmatchings/F344", func_8001F854);

void func_8001F990(void)
{
    StHEADER*   header;
    CdCmdQueue* p;

    p = &CdCmd_Queue;
    if (D_8006AC1A != 0) {
        if (DecDCTvlc2(NULL, NULL, D_8006AC38) == 0) {
            func_8001F854();
        }
        return;
    }

    if (StGetNext(&D_8006AC68, (u_long**)&header) != 0) {
        return;
    }

    if (header->frameCount >= (u32)D_8006AC0C) {
        func_80026218(0);
        p->field_1F6 = 1;
        p->field_1E4 = 4;
    }

    p->field_242 = 0;
    if (D_8006AC12 != 0) {
        DecDCTvlcSize2(0);
    } else {
        DecDCTvlcSize2(DecDCTBufSize(D_8006AC68) / 2 + 2);
    }

    if (DecDCTvlc2(D_8006AC68, D_8006AC50[D_8005EAEC], D_8006AC38) == 0) {
        func_8001F854();
    } else {
        D_8006AC1A = 1;
    }

    if (p->field_1E8 == 0) {
        p->field_1E4 = 4;
    }
}

INCLUDE_ASM("main/nonmatchings/F344", func_8001FAE0);

INCLUDE_ASM("main/nonmatchings/F344", func_80020058);

GStruct24* func_80020278(u32 arg0)
{
    return &D_8006D4F0[arg0 & 0xFFFF];
}

void func_80020298(s16 arg0)
{
    RECT        rect;
    CdCmdQueue* p;

    p = &CdCmd_Queue;
    func_80041E4C();
    Mem_SetActiveAuxHeap(0);
    Mem_InitAux();
    if (D_80070F68.field_12a == 0) {
        D_8006AC40 = Mem_Malloc(0x4A800, 1);
    } else {
        D_8006AC40 = Mem_Malloc(0x45400, 1);
    }
    if ((arg0 & 0xFFFF) != 0) {
        rect.x = 0x140;
        rect.y = 0;
        rect.w = 0xA0;
        rect.h = 0x100;
        MoveImage2(&rect, 0x2C0, 0);
        rect.x = 0x140;
        rect.y = 0x100;
        rect.w = 0xA0;
        rect.h = 0x100;
        MoveImage2(&rect, 0x360, 0);
    }
    D_8006AC1E   = arg0;
    p->field_244 = 1;
}

void func_80020388(void)
{
    D_8006AC28 = 0;
}

s16 func_80020394(void* arg0)
{
    s32 i;
    s32 result;

    i      = 0;
    result = 0;
    while (1) {
        if (D_8006D4F0[i & 0xFFFF].field_0 == 1) {
            if (D_8006D4F0[i & 0xFFFF].field_E < 0x64U) {
                if (D_8006D4F0[i & 0xFFFF].field_4 != 0) {
                    result = 1;
                    break;
                }
            }
        }
        i = i + 1;
        if ((u32)(i & 0xFFFF) >= 0xFU) {
            break;
        }
    }
    return result;
}

u16 func_80020414(u32 arg0)
{
    return D_8006D4F0[arg0 & 0xFFFF].field_1A;
}

void func_8002043C(u32 arg0)
{
    func_8001F180(arg0 & 0xFFFF);
}
