#include "common.h"

#include <psyq/libpress.h>

#include "main/unknown_syms.h"
#include "main/fs.h"
#include "main/stream.h"
#include "main/tmd.h"
#include "main/wipsys.h"

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

u32 func_8001F180(u32 arg0)
{
    u8          params[8];
    RECT        clearRect;
    RECT*       rect;
    RECT*       firstImage;
    RECT*       secondImage;
    CdCmdQueue* queue;
    u32         slot;

    slot             = arg0 & 0xFFFF;
    queue            = &CdCmd_Queue;
    queue->field_1F2 = 0;
    queue->field_1E4 = 0;
    queue->field_1F6 = 0;
    queue->field_1F4 = 0;
    Stream_InitFromSlot(slot);
    if (D_8006AC58 != 0) {
        if (D_8006AC30.sector == 0) {
            return 1U;
        }
        D_8006AC08 = Stream_Slots[slot].field_8 + D_8006AC30.sector;
    }
    if (D_8006AC14 != 0) {
        params[3] = 0xFF;
        Mdec_SetupBuffers(params);
        rect             = &clearRect;
        queue->field_1EA = 1;
        clearRect.y      = 0;
        clearRect.x      = 0;
        if (D_8006AC14 == 1) {
            rect->w = 0x1E0;
        } else {
            rect->w = 0x140;
        }
        /* Keep each call argument as a fresh stack-address calculation. */
        firstImage = &clearRect;
        TOUCH_REG(firstImage);
        rect->h = 0xF0;
        ClearImage(firstImage, 0U, 0U, 0U);
        secondImage = &clearRect;
        TOUCH_REG(secondImage);
        rect->y = 0x110;
        ClearImage(secondImage, 0U, 0U, 0U);
        if (D_8006AC14 == 1) {
            Display_SetMode(0xF010);
        } else {
            Display_SetMode(0xD010);
        }
        Display_State.field_106 = 1;
        DecDCTvlcBuild(D_8006AC38);
        return 0U;
    }
    Mdec_SetupBuffers(&Game_Session->field_4);
    return 0U;
}

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
            if (Display_State.frameMode != 0) {
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
            SOFT_TOUCH_REG2(p, flipped);
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
    CdCmdQueue* p;
    s32         size;
    u_long**    base;
    u_long**    outs;
    u16         ac6c;
    s32         temp;

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

static __inline__ u16 Stream_SeekPosition(u8* loc)
{
    if (((s32 (*)())CdCmd_SeekL)(loc, 0) & 0xFFFF) {
        return 1;
    }
    return 0;
}

s32 func_8001FAE0(u16 arg0, s32 arg1)
{
    RECT        rect;
    CdCmdQueue* state;
    CdCmdQueue* setup;
    CdCmdQueue* stop;
    CdCmdQueue* restart;
    s32         sector;
    u16         ready;
    u32         volumeAddress;
    u32         volume;
    s32         mode;
    s32         videoMode;
    s32         displayMode;

    state = &CdCmd_Queue;
    switch (state->field_1E4) {
        case 0:
            if (*(u16*)&D_8006AC5C == 0) {
                state->field_248 = 0;
                state->field_244 = 1;
            }
            state->field_24C = 0;
            switch ((s16)CdCmd_PollStatus(0, 0)) {
                case 0:
                    break;
                case 2:
                    CdFlush();
                case 1:
                    state->field_1E4++;
                    break;
            }
            break;
        case 1:
            setup = &CdCmd_Queue;
            DecDCTReset(0);
            StSetStream(D_8006AC14 == 2 ? 0 : D_8006AC14, 0, -1, NULL, NULL);
            StSetRing((u_long*)D_8006AC60, D_8006AC24);
            StClearRing();
            Wip_SysFlags.field_6 = 1;
            DecDCToutCallback(Mdec_UploadSlice);
            CdVol_ApplyFromTable(0);
            setup->field_1EC = 0;
            D_8006AC1A       = 0;
            state->field_1D6 = 0;
            state->field_1E4++;
        case 2:
            if ((arg0 & 0xFFFF) == 0) {
                sector = D_8006AC08 + arg1;
            } else {
                sector = arg1;
            }
            state->field_242 = 1;
            state->field_24E = 1;
            CdIntToPos(sector, (CdlLOC*)&rect);
            ready = Stream_SeekPosition((u8*)&rect);
            TOUCH_REG(ready);
            if (ready & 0xFFFF) {
                CdVol_ApplyFromTable((u8)D_8006AC58);
                mode = D_8006AC58 != 0 ? 0x1E0 : 0x1A0;
                if (!(CdRead2(mode) & 0xFFFF)) {
                    D_8006AC20       = 1;
                    state->field_1E4 = 9;
                    return 0;
                }
                state->field_242 = 0;
                state->field_24E = 0;
                state->field_1E4++;
                if (*(u16*)&D_8006AC5C != 0) {
                    CdCmd_ClearBusy();
                }
            }
            break;
        case 3:
            Mdec_DecodeFrame();
            if (CdSync_IsShellOpenBitSet() != 0) {
                state->field_24C = 1;
                CdCmd_SetBusy();
                state->field_1E4 = 6;
            }
            break;
        case 4:
            Mdec_DecodeFrame();
            state->field_242 = 1;
            if (CdCmd_PausePoll() & 0xFFFF) {
                state->field_1E4++;
            }
            break;
        case 5:
            if (D_8006AC16 == 1) {
                CdCmd_SetBusy();
                state->field_1EA = 1;
                state->field_1E4 = 1;
                return 0;
            }
            state->field_242 = 0;
            stop             = &CdCmd_Queue;
            DecDCToutCallback(NULL);
            DecDCTReset(0);
            StClearRing();
            StUnSetRing();
            videoMode            = D_8006AC14;
            Wip_SysFlags.field_6 = 0;
            stop->field_24A      = 0;
            stop->field_1FA      = 0;
            stop->field_1F4      = 0;
            stop->field_1E2      = 0;
            stop->field_1E4      = 0;
            if (videoMode != 0) {
                displayMode = Display_State.field_12a;
                if (displayMode == 1) {
                    rect.y = 0;
                    rect.x = 0;
                    if (videoMode == displayMode) {
                        rect.w = 0x1E0;
                    } else {
                        rect.w = 0x140;
                    }
                    rect.h = 0xF0;
                    ClearImage(&rect, 0, 0, 0);
                    rect.y = 0x110;
                    ClearImage(&rect, 0, 0, 0);
                    Display_SetMode(0xD010);
                }
                stop->field_1E6         = 0;
                Display_State.field_106 = 0;
            } else if (D_8006AC3C != 0) {
                stop->field_244 = 0;
            }
            return 1;
        case 6:
            if (CdCmd_RecoverDisk() != 0) {
                state->field_1E4++;
            }
            break;
        case 7:
            if ((s16)CdCmd_StopMdec(0) != 0) {
                if (D_8006AC14 != 0) {
                    if (D_8006AC14 == 1) {
                        Display_SetMode(0xF010);
                    }
                    Display_State.field_106 = 1;
                }
                state->field_242 = 1;
                state->field_1E4 = 8;
            }
            break;
        case 8:
            CdIntToPos(D_8006AC08 + (state->field_1EA - 1) * 10, (CdlLOC*)&rect);
            ready = Stream_SeekPosition((u8*)&rect);
            TOUCH_REG(ready);
            if (ready & 0xFFFF) {
                mode = D_8006AC58 != 0 ? 0x1E0 : 0x1A0;
                if (!(CdRead2(mode) & 0xFFFF)) {
                    D_8006AC20       = 8;
                    state->field_1E4 = 9;
                    return 0;
                }
                restart = &CdCmd_Queue;
                DecDCTReset(0);
                StSetStream(D_8006AC14 == 2 ? 0 : D_8006AC14, 0, -1, NULL, NULL);
                StSetRing((u_long*)D_8006AC60, D_8006AC24);
                StClearRing();
                Wip_SysFlags.field_6 = 1;
                DecDCToutCallback(Mdec_UploadSlice);
                CdVol_ApplyFromTable(0);
                __asm__("lui %0, %%hi(D_8006AC58)\n\tlbu %1, %%lo(D_8006AC58)(%0)"
                        : "=&r"(volumeAddress), "=r"(volume) : : "memory");
                restart->field_1EC = 0;
                D_8006AC1A         = 0;
                ((void (*)())CdVol_ApplyFromTable)(volume);
                state->field_24C = 0;
                state->field_242 = 0;
                state->field_1E4 = 3;
            }
            break;
        case 9:
            if ((s16)CdCmd_StopMdec(0) != 0) {
                state->field_1E4 = D_8006AC20;
            }
            break;
    }
    return 0;
}

static __inline__ void Stream_UploadFrameStrips(RECT* rect, u32 x, u32 y, u16 useDisplayBuffer)
{
    s16 stripWidth;
    s32 bufferY;
    s32 stride;
    u16 i;
    u8* data;
    u32 frameWidth;

    stripWidth = 0x10;
    if (D_8006AC14 == 1) {
        stripWidth = 0x18;
    }
    if (useDisplayBuffer & 0xFFFF) {
        bufferY = y & 0xFFFF;
        if (Display_State.field_1f != 0) {
            bufferY += 0x110;
        }
        rect->y = bufferY;
    } else {
        rect->y = y;
    }
    rect->w    = stripWidth;
    rect->x    = x;
    rect->h    = (s16)D_8006AC6C;
    stride     = stripWidth * D_8006AC6C * 2;
    data       = (u8*)D_8006AC48[D_8005EAEE];
    frameWidth = D_8006AC5A;
    for (i = 0; (u32)(i & 0xFFFF) < (frameWidth >> 4); i++) {
        LoadImage(rect, (u_long*)data);
        rect->x = (u16)rect->x + stripWidth;
        data   += stride;
    }
}

void func_80020058(void)
{
    RECT        rect;
    s32         yOffset;
    CdCmdQueue* queue;
    s32         useDisplayBuffer;
    s32         x;
    s32         y;

    queue = &CdCmd_Queue;
    if ((queue->field_20A == 0) && (queue->field_1E6 != 0)) {
        if (queue->field_1EA == D_8006AC0C) {
            queue->field_1F2 = 0;
        } else if (queue->field_1F4 != 0) {
            queue->field_1F4 = 0;
            queue->field_1F2 = 0;
        } else {
            queue->field_1F2 = 1;
        }
        if (queue->field_22C == 0) {
            useDisplayBuffer = D_8006AC18 != 1;
            x                = D_8006AC0E;
            y                = D_8006AC10;
            Stream_UploadFrameStrips(&rect, x, y, useDisplayBuffer);
        } else {
            rect.x = queue->field_230;
            rect.y = queue->field_232;
            rect.w = (s16)D_8006AC5A;
            rect.h = (s16)D_8006AC6C;
            if (D_8006AC18 == 1) {
                yOffset = 0;
            } else {
                yOffset = Display_State.field_1f != 0 ? 0x110 : 0;
            }
            MoveImage(&rect, (s32)D_8006AC0E, yOffset + D_8006AC10);
        }
    }
}

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

void Stream_ResetRestoreState(void)
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

u16 Stream_GetSlotField1A(u32 arg0)
{
    return Stream_Slots[arg0 & 0xFFFF].field_1A;
}

void Stream_KickDecode(u32 arg0)
{
    func_8001F180(arg0 & 0xFFFF);
}
