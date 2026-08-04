#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libpress.h>

#include "main/fs.h"
#include "main/game.h"
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

INCLUDE_ASM("main/nonmatchings/F344", func_8001F990);

INCLUDE_ASM("main/nonmatchings/F344", func_8001FAE0);

INCLUDE_ASM("main/nonmatchings/F344", func_80020058);

GStruct24* func_80020278(u32 arg0)
{
    return &D_8006D4F0[arg0 & 0xFFFF];
}

INCLUDE_ASM("main/nonmatchings/F344", func_80020298);

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
