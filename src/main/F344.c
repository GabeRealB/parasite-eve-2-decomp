#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libpress.h>

#include "main/boot.h"
#include "main/fs.h"
#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/F344", func_8001EB44);

void func_8001ED20(u32 arg0)
{
    GStruct24* base;
    GStruct24* entry;

    CdCmd_Queue.field_20A = 0;
    base                  = D_8006D4F0;
    D_8006AC12            = 0;
    entry                 = &base[arg0 & 0xFFFF];
    D_8006AC08            = entry->field_4;
    D_8006AC0C            = entry->field_1A;
    D_8006AC5A            = entry->field_12;
    D_8006AC6C            = entry->field_14;
    D_8006AC0E            = entry->field_16;
    D_8006AC10            = entry->field_18;
    D_8006AC16            = entry->field_1C;
    D_8006AC14            = entry->field_22;
    D_8006AC58            = entry->field_24;
    D_8006AC18            = entry->field_26;
}

s16 func_8001EDC8(u8* arg0, s32 arg1, s32 arg2)
{
    s32        i;
    s32        found;
    s32        result;
    GStruct24* base;
    s32        one;
    s32        ret;

    result = 0;
    i      = result;
    found  = result;
    base   = D_8006D4F0;
    one    = 1;
    arg2  &= 0xFFFF;

loop:
    if (base[i & 0xFFFF].field_0 == one) {
        if (base[i & 0xFFFF].field_4 != 0) {
            if (base[i & 0xFFFF].field_E == arg0[0]) {
                if (base[i & 0xFFFF].field_10 == (arg1 & 0xFFFF)) {
                    if (base[i & 0xFFFF].field_C == 0) {
                        if (arg2 == 0) {
                            goto matched;
                        }
                        if (base[i & 0xFFFF].field_1E != 0) {
                            found = 1;
                            goto matched_result;
                        }
                    } else if (base[i & 0xFFFF].field_C == arg0[1]) {
                        if (arg2 == 0) {
                            goto matched;
                        }
                        if (base[i & 0xFFFF].field_1E == 0) {
                            goto done;
                        }
                        found = 1;
                        goto matched_result;
                    }
                }
            }
        }
    }
    i = i + 1;
    if ((u32)(i & 0xFFFF) < 0xFU) {
        goto loop;
    }
done:
    if ((found & 0xFFFF) == 0) {
        goto ret_neg;
    }
    ret = result << 0x10;
    return ret >> 0x10;

matched:
    found = 1;
matched_result:
    result = i;
    goto done;

ret_neg:
    return -1;
}

s16 func_8001EED8(u8* arg0)
{
    s32        i;
    GStruct24* base;
    s32        one;
    s32        ret;

    i    = 0;
    base = D_8006D4F0;
    one  = 1;
    while (1) {
        if (base[i & 0xFFFF].field_0 == one) {
            if (base[i & 0xFFFF].field_E == arg0[0]) {
                if (base[i & 0xFFFF].field_C == 0) {
                    if (base[i & 0xFFFF].field_1E != 0) {
                        ret = i << 0x10;
                        return ret >> 0x10;
                    }
                }
                if (base[i & 0xFFFF].field_C == arg0[1]) {
                    if (base[i & 0xFFFF].field_1E != 0) {
                        ret = i << 0x10;
                        return ret >> 0x10;
                    }
                }
            }
        }
        i = i + 1;
        if ((u32)(i & 0xFFFF) >= 0xFU) {
            break;
        }
    }
    return -1;
}

s32 func_8001EF9C(s32 arg0, s32 arg1)
{
    RECT        rect;
    u8          param1[8];
    u8          param2[8];
    GStruct14*  g;
    CdCmdQueue* p;
    s32         state;
    u8          f7;
    u8          f6;
    u8          f74;

    p     = &CdCmd_Queue;
    state = D_8006AC28;
    if (state != 1) {
        if (state < 2) {
            if (state == 0) {
                goto case0;
            }
            goto ret_zero;
        }
        if (state == 2) {
            goto ret_one;
        }
        goto ret_zero;
    case0:
        if (D_8006AC1E != 0) {
            rect.x = 0x2C0;
            rect.y = 0;
            rect.w = 0xA0;
            rect.h = 0x100;
            MoveImage2(&rect, 0x140, 0);
            rect.x = 0x360;
            rect.y = 0;
            rect.w = 0xA0;
            rect.h = 0x100;
            MoveImage2(&rect, 0x140, 0x100);
        }
        func_800144F8((s32)D4F564_8005ED64->field_7, (s32)D4F564_8005ED64->field_6);
        if ((arg0 & 0xFFFF) == 1) {
            Mem_SetActiveAuxHeap(1);
        }
        func_80041EB4();
        if (Display_State.field_12a == 1) {
            func_8001D39C();
            func_8001D498();
        }
        D_8006AC28 = D_8006AC28 + 1;
        if (arg1 & 0xFFFF) {
            g         = D4F564_8005ED64;
            f7        = g->field_7;
            param1[3] = f7;
            f6        = g->field_6;
            param1[0] = 0;
            param1[2] = f6;
            f74       = g->field_74;
            param2[1] = 5;
            param2[2] = 0;
            param2[3] = 0;
            param2[0] = f74;
            CdCmd_Enqueue(0x21, param1, param2);
            goto ret_zero;
        }
        goto ret_one;
    }
    if (func_8001D344() & 0xFFFF) {
        p->field_244 = 0;
        D_8006AC28   = D_8006AC28 + 1;
        return 1;
    }
    goto ret_zero;

ret_one:
    return 1;
ret_zero:
    return 0;
}

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
            f12a = Display_State.field_12a;
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
            p->field_1E6            = 0;
            Display_State.field_106 = 0;
        } else if (D_8006AC3C != 0) {
            p->field_244 = 0;
        }
        return 1;
    }
    return 0;
}

INCLUDE_ASM("main/nonmatchings/F344", func_8001F430);

INCLUDE_ASM("main/nonmatchings/F344", func_8001F6B8);

void func_8001F854(void)
{
    CdCmdQueue*       p;
    s32               size;
    register u_long** base asm("v1");
    u_long**          outs;
    u16               ac6c;
    s32               temp;

    p = &CdCmd_Queue;
    StFreeRing(D_8006AC68);
    DecDCTin(D_8006AC50[D_8005EAEC], D_8006AC14 == 2 ? 0 : D_8006AC14);
    if (D_8006AC14 != 0) {
        base = D_8006AC48;
        outs = &base[D_8005EAEE ^ 1];
        ac6c = D_8006AC6C;
        if (D_8006AC14 == 1) {
            size = ac6c * 12;
        } else {
            size = ac6c * 8;
        }
        DecDCTout(*outs, size);
    } else {
        temp = D_8006AC6C;
        size = (D_8006AC5A * temp) / 2;
        base = D_8006AC48;
        DecDCTout(base[D_8005EAEE ^ 1], size);
    }
    p->field_1EC = 1;
    D_8006AC1A   = 0;
    D_8005EAEC  ^= 1;
}

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
    if (Display_State.field_12a == 0) {
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
