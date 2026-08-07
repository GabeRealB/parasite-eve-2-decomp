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

void Mdec_SetupBuffers(u8* arg0)
{
    s32     temp_lo;
    u16*    temp_v1;
    u16*    temp_v1_2;
    u_long* temp_v1_3;
    u_long* temp_v1_4;
    s32     temp_lo_2;
    u16*    temp_v1_5;

    D_8006AC5C            = 0;
    D_8006AC3C            = 1;
    CdCmd_Queue.field_24A = 0;
    CdCmd_Queue.field_22C = 0;
    D_8006AC24            = 0x20;
    D_8006AC38            = D_8005C36C;
    D_8006AC60            = (u16*)((u8*)D_8005C36C + 0x11000);
    D_8006AC64            = D_8006AC40;

    switch ((s8)(arg0[3] + 1)) {
        case 0:
            D_8006AC24 = 0x28;
            D_8006AC38 = (u_short*)D_8006AC40;
            temp_v1    = (D_8006AC60 = (u16*)((u8*)D_8006AC40 + 0x11000));
            {
                u16 h         = D_8006AC6C;
                s32 stride    = h * 0x30;
                temp_lo       = D_8006AC5A * h;
                D_8006AC48[1] = (u_long*)(temp_v1_2 = (u16*)((u8*)temp_v1 + 0x14000));
                D_8006AC48[0] = (u_long*)temp_v1_2;
                D_8006AC50[0] = (temp_v1_3 = (u_long*)((u8*)temp_v1_2 + stride));
                D_8006AC50[1] = (temp_v1_4 = (u_long*)((u8*)temp_v1_3 + temp_lo));
                D_8006AC44    = (u8*)temp_v1_4 + temp_lo;
            }
            return;
        case 1: {
            u_long** p50;
            u_long** p48;
            void*    base;

            temp_lo_2 = D_8006AC5A * D_8006AC6C;
            p50       = D_8006AC50;
            temp_v1_5 = (u16*)((u8*)D_8006AC60 + 0x10000);
            base      = D_8006AC40;
            p48       = D_8006AC48;
            p50[0]    = (u_long*)temp_v1_5;
            p48[0]    = (u_long*)base;
            p50[1]    = (u_long*)((u8*)temp_v1_5 + temp_lo_2);
            p48[1]    = (u_long*)((u8*)base + (temp_lo_2 * 2));
            return;
        }
        case 2:
            func_80179988(arg0);
            return;
        case 3:
            func_80179954(arg0);
            return;
        case 6:
            func_801799BC(arg0);
            return;
    }
}

void Stream_InitFromSlot(u32 arg0)
{
    StreamSlot* base;
    StreamSlot* entry;

    CdCmd_Queue.field_20A = 0;
    base                  = Stream_Slots;
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

s16 Stream_FindSlot(u8* arg0, s32 arg1, s32 arg2)
{
    s32         i;
    s32         found;
    s32         result;
    StreamSlot* base;
    s32         one;
    s32         ret;

    result = 0;
    i      = result;
    found  = result;
    base   = Stream_Slots;
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

s16 Stream_FindSlotByKey(u8* arg0)
{
    s32         i;
    StreamSlot* base;
    s32         one;
    s32         ret;

    i    = 0;
    base = Stream_Slots;
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

s32 Stream_RestoreAfterLoad(s32 arg0, s32 arg1)
{
    RECT         rect;
    u8           param1[8];
    u8           param2[8];
    GameSession* g;
    CdCmdQueue*  p;
    s32          state;
    u8           f7;
    u8           f6;
    u8           f74;

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
        Mem_ConfigureAuxHeap((s32)Game_Session->field_7, (s32)Game_Session->field_6);
        if ((arg0 & 0xFFFF) == 1) {
            Mem_SetActiveAuxHeap(1);
        }
        Tmd_AllocMissingBuffers();
        if (Display_State.field_12a == 1) {
            CdCmd_BuildVlcIfStream();
            CdCmd_SelectMdecBuffer();
        }
        D_8006AC28 = D_8006AC28 + 1;
        if (arg1 & 0xFFFF) {
            g         = Game_Session;
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
    if (CdCmd_IsIdle() & 0xFFFF) {
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

INCLUDE_ASM("main/nonmatchings/stream", func_8001F180);

s32 CdCmd_StopMdec(s32 arg0)
{
    RECT        rect;
    s32         ac14;
    s32         f12a;
    CdCmdQueue* p;

    if (CdCmd_PausePoll() & 0xFFFF) {
        p = &CdCmd_Queue;
        DecDCToutCallback(0);
        DecDCTReset(0);
        StClearRing();
        StUnSetRing();
        ac14                 = D_8006AC14;
        Wip_SysFlags.field_6 = 0;
        p->field_24A         = 0;
        p->field_1FA         = 0;
        p->field_1F4         = 0;
        p->field_1E2         = 0;
        p->field_1E4         = 0;
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
                Display_SetMode(0xD010);
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

INCLUDE_ASM("main/nonmatchings/stream", func_8001F430);

void Mdec_UploadSlice(void)
{
    RECT              rect;
    s32               index;
    s32               w;
    u16               y;
    s32               new_val;
    register u_long** base asm("s1");
    s32               mask;
    u_long**          p;
    s32               size;
    s32               t;
    s32               flipped;
    s32               x;
    u16               ac0e;

    if (D_8006AC14 != 0) {
        if ((D_8006AC14 == 1) && (StCdIntrFlag != 0)) {
            StCdInterrupt();
            StCdIntrFlag = 0;
        }
        if (D_8006AC1C != ((D_8006AC5A >> 4) - 1)) {
            new_val    = D_8006AC1C + 1;
            D_8006AC1C = new_val;
            mask       = 0xFFFF;
            index      = (new_val & mask) - 1;
            ac0e       = D_8006AC0E;
            if (D_8006AC14 == 1) {
                x = ac0e + (index * 0x18);
            } else {
                x = ac0e + (index * 0x10);
            }
            y      = D_8006AC10;
            rect.x = x;
            if (Display_State.field_118 != 0) {
                y += 0x110;
            }
            w      = 0x10;
            rect.y = y;
            if (D_8006AC14 == 1) {
                w = 0x18;
            }
            base   = D_8006AC48;
            rect.h = D_8006AC6C;
            rect.w = w;
            LoadImage(&rect, base[D_8005EAEE ^ 1]);
            t       = D_8005EAEE;
            size    = D_8006AC6C;
            flipped = t ^ 1;
            p       = &base[t & mask];
            asm("" : "+r"(p), "+r"(flipped));
            D_8005EAEE = flipped;
            if (D_8006AC14 == 1) {
                size *= 0xC;
            } else {
                size *= 8;
            }
            DecDCTout(*p, size);
            return;
        }
    }
    func_8001F430();
}

void Mdec_KickStrip(void)
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

void Mdec_DecodeFrame(void)
{
    StHEADER*   header;
    CdCmdQueue* p;

    p = &CdCmd_Queue;
    if (D_8006AC1A != 0) {
        if (DecDCTvlc2(NULL, NULL, D_8006AC38) == 0) {
            Mdec_KickStrip();
        }
        return;
    }

    if (StGetNext(&D_8006AC68, (u_long**)&header) != 0) {
        return;
    }

    if (header->frameCount >= (u32)D_8006AC0C) {
        CdVol_ApplyFromTable(0);
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
        Mdec_KickStrip();
    } else {
        D_8006AC1A = 1;
    }

    if (p->field_1E8 == 0) {
        p->field_1E4 = 4;
    }
}

INCLUDE_ASM("main/nonmatchings/stream", func_8001FAE0);

INCLUDE_ASM("main/nonmatchings/stream", func_80020058);

StreamSlot* Stream_GetSlot(u32 arg0)
{
    return &Stream_Slots[arg0 & 0xFFFF];
}

void Mem_AllocAuxWithImages(s16 arg0)
{
    RECT        rect;
    CdCmdQueue* p;

    p = &CdCmd_Queue;
    Gpu_ResetGraphAndOt();
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

s16 Stream_HasActiveLowId(void* arg0)
{
    s32 i;
    s32 result;

    i      = 0;
    result = 0;
    while (1) {
        if (Stream_Slots[i & 0xFFFF].field_0 == 1) {
            if (Stream_Slots[i & 0xFFFF].field_E < 0x64U) {
                if (Stream_Slots[i & 0xFFFF].field_4 != 0) {
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
    return Stream_Slots[arg0 & 0xFFFF].field_1A;
}

void func_8002043C(u32 arg0)
{
    func_8001F180(arg0 & 0xFFFF);
}
