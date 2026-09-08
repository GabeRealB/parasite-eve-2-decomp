#include "common.h"

#include <psyq/libapi.h>

#include "main/unknown_syms.h"
#include "main/cdstream.h"

s32 func_80059EE0(CdReadyEntry* arg0)
{
    struct {
        CdlLOC       loc;
        u8           pad[4];
        s8           mode[8];
        u8           result[8];
        AsyncCbEntry entry;
    } sp;
    s32 phase;
    s32 modeSync;
    s32 locationSync;
    s32 readSync;
    s32 pauseSync;
    u32 flags;
    u32 modeFlags;
    u32 irqAddr1;
    u32 irqAddr2;
    u32 irqOffset;
    u32 initFlags;
    u32 errorCode;

    flags = arg0->flags;
    if ((flags >> 1) & 1) {
        arg0->flags = flags & ~2;
        D_800827E8  = 0;
        if (!(CdStream_State.flags & 1) && CdStream_State.pending == 0) {
            if ((CdStream_State.flags1 >> 1) & 1) {
                initFlags   = arg0->flags & ~0x10;
                initFlags  &= ~0x1FE0;
                arg0->flags = initFlags | 0x80;
            } else {
                if (!(CdMode() & 0x80)) {
                    arg0->flags |= 0x10;
                } else {
                    arg0->flags &= ~0x10;
                }
                arg0->flags = (*(volatile u32*)&arg0->flags & ~0x1FE0) | 0x20;
            }
        } else {
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x80;
            if (CdStream_State.pending != 0) {
                AsyncCb_Cancel((s16)CdStream_State.pending);
            }
            sp.entry.field_8       = (s32 (*)(AsyncCbEntry*))CdStream_InitDisc;
            sp.entry.field_C       = (void (*)(AsyncCbEntry*))CdStream_MarkEnding;
            sp.entry.field_10      = (s32 (*)(AsyncCbEntry*))CdStream_Flush;
            CdStream_State.pending = func_8004DE18(&sp.entry);
        }
    }
    D_8008280C = (arg0->flags >> 5) & 0xFF;
    if (CdStream_State.pending != 0) {
        return 0;
    }
    phase = (arg0->flags >> 5) & 0xFF;
    switch (phase) {
        case 1:
            sp.mode[0] = -0x60;
        retry_mode:
            CdControlF(CdlSetmode, (u8*)&sp.mode[0]);
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x40;
            goto phase_advanced;
        case 2:
            modeSync = CdSync(1, &sp.result[0]);
            if (modeSync == CdlDiskError) {
                D_80068B64 += 1;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)modeSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    /* Keep each disk-error path separate before the shared counter update. */
                    SOFT_BARRIER();
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)modeSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    SOFT_BARRIER();
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (D_800827E8++ < 0x258) {
                    goto retry_mode;
                }
                goto stream_error;
            }
            CdStream_State.flags1 |= 2;
            modeFlags              = arg0->flags;
            if (!((modeFlags >> 4) & 1)) {
                arg0->flags = ((u32)~0x1FE0 & modeFlags) | 0x80;
            } else {
                arg0->flags = (modeFlags & ~0x1FE0) | 0x60;
                D_800827E8  = 3;
                case 3:
                    if (--D_800827E8 != 0) {
                        goto phase_advanced;
                    }
                    arg0->flags = (arg0->flags & ~0x1FE0) | 0x80;
            }
            D_800827E8 = 0;
        case 4:
            D_80068B54              = 0;
            CdStream_State.field_4C = 5;
            CdStream_State.field_2C = arg0->sectorPos;
        set_location:
            CdIntToPos(arg0->sectorPos, &sp.loc);
            CdControlF(CdlSetloc, &sp.loc.minute);
            D_800827E8  = 0;
            arg0->flags = (arg0->flags & ~0x1FE0) | 0xA0;
        case 5:
            locationSync = CdSync(1, &sp.result[0]);
            if (locationSync == CdlDiskError) {
                D_80068B64 += 1;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)locationSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    SOFT_BARRIER();
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)locationSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    SOFT_BARRIER();
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (D_800827E8++ < 0x258) {
                    goto set_location;
                }
                goto stream_error;
            }
            if (locationSync != CdlComplete) {
                goto wait_for_progress;
            }
            arg0->flags = (arg0->flags & ~0x1FE0) | 0xC0;
        case 6:
            D_800827E8 = 0;
        start_read:
            CdReady_InstallCallback((CdlCB)CdStream_ReadyMts);
            CdStream_State.remaining = 0;
            CdStream_State.field_4C  = 1;
            CdControlF(CdlReadN, NULL);
            arg0->flags = (arg0->flags & ~0x1FE0) | 0xE0;
            goto phase_advanced;
        case 7:
            D_80068B54 = 0;
            readSync   = CdSync(1, &sp.result[0]);
            if (readSync == CdlDiskError) {
                CdReady_ClearCallback();
                arg0->flags = (arg0->flags & ~0x1FE0) | 0xC0;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)readSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    SOFT_BARRIER();
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)readSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    SOFT_BARRIER();
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (D_800827E8++ < 0x258) {
                    goto start_read;
                }
                goto stream_error;
            }
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x100;
        case 8:
            if (CdStream_State.field_4C == 1) {
                goto wait_for_progress;
            }
            if (CdStream_State.field_4C == 0) {
                goto wait_for_progress;
            }
            if (CdStream_State.field_4C == 4) {
                CdStream_State.flags0 |= 4;
                CdReady_ClearCallback();
                if (CdStream_State.field_1C & 1) {
                    irqOffset = ((CdStream_State.ringHalf + 0x3F) & ~0x3F);
                    irqAddr1  = CdStream_State.spuBase + irqOffset;
                    if (D_80068B5C != 0) {
                        SpuSetIRQ(0);
                        SpuSetIRQCallback(NULL);
                    }
                    D_80068B5C = 1;
                    SpuSetIRQCallback(CdStream_SpuIrqHandler);
                    SpuSetIRQAddr(irqAddr1);
                    SpuSetIRQ(1);
                } else if (!((CdStream_State.flags0 >> 4) & 1)) {
                    CdStream_State.flags0 |= 0x20;
                } else if (CdStream_State.field_1C != 0) {
                    irqAddr2 = CdStream_State.spuBase + 0x40;
                    if (D_80068B5C != 0) {
                        SpuSetIRQ(0);
                        SpuSetIRQCallback(NULL);
                    }
                    D_80068B5C = 1;
                    SpuSetIRQCallback(CdStream_SpuIrqHandler);
                    SpuSetIRQAddr(irqAddr2);
                    SpuSetIRQ(1);
                }
            } else if (CdStream_State.field_4C != 2) {
                if (CdStatus() & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = 5;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (D_80082808 == 0) {
                    D_80082808 = 9;
                }
                CdStream_State.flags2 |= 2;
                CdStream_State.flags0 &= 0xFB;
            } else {
                goto wait_for_progress;
            }
            D_800827E8              = 0;
            CdStream_State.field_4C = 0;
            CdReady_ClearCallback();
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x120;
        case 9:
            D_800827E8 = 0;
        pause_read:
            CdControlF(CdlPause, NULL);
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x160;
            goto phase_advanced;
        case 11:
            D_80068B54 = 0;
            pauseSync  = CdSync(1, &sp.result[0]);
            if (pauseSync == CdlDiskError) {
                D_80068B64 += 1;
                if ((sp.result[0] & CdlStatShellOpen) || (CdStatus() & CdlStatShellOpen)) {
                    CdStream_State.flags |= 1;
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)pauseSync;
                    }
                    D_80082810 = D_80082808;
                    goto stream_error;
                }
                arg0->flags = (arg0->flags & ~0x1FE0) | 0x120;
                if (D_800827E8 >= 0x258) {
                    goto stream_error;
                }
                D_800827E8 += 1;
                goto pause_read;
            }
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x180;
        case 12:
            if (SpuIsTransferCompleted(0) == 0) {
                goto wait_for_progress;
            }
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x140;
        case 10:
            if (CdSync(1, &sp.result[0]) == 0) {
                goto wait_for_progress;
            }
            D_8008280C = 0;
            return 1;
    }
phase_advanced:
    D_80082814 = (arg0->flags >> 5) & 0xFF;
    return 0;
wait_for_progress:
    D_80082814 = (arg0->flags >> 5) & 0xFF;
    if (++D_800827E8 < 0x259) {
        return 0;
    }
stream_error:
    CdStream_State.flags2 |= 2;
    if (D_80082808 == 0) {
        errorCode = (arg0->flags >> 5) & 0xFF;
        /* Keep the phase extraction separate from the error-code shift. */
        SOFT_USE_REG(errorCode);
        errorCode *= 0x10;
        D_80082808 = errorCode | 0xA;
    }
    CdStream_State.field_4C = 0;
    CdReady_ClearCallback();
    CdFlush();
    return 1;
}

void CdStream_ReadyMts(s32 interrupt, u8* result)
{
    volatile CdStreamState* state;
    s16                     chunkSectors;
    s32                     intr;
    s32                     skipIndex;
    s32                     skipStamp;
    s32                     timer;
    s32                     sectorPos;
    s32                     attributes;
    s32                     writeSize;
    u32                     timerPhase;
    u32                     previousPhase;
    volatile CdStreamState* channelState;
    volatile CdStreamState* regionState;
    CdStreamChannels*       channels;
    s32                     channelBase;
    s32                     channelCount;

    if ((u16)D_80068B6A != 0) {
        D_80082808             = 0xC;
        D_80082810             = D_80082808;
        CdStream_State.flags2 |= 2;
        return;
    }
    D_80068B6A = 1;
    intr       = interrupt & 0xFF;
    if (intr == 1) {
        if ((s16)(u16)CdStream_State.field_4C != 2) {
            if ((s16)(u16)CdStream_State.field_4C == intr) {
                if (SpuIsTransferCompleted(0) == 0) {
                    D_80068B62 += 1;
                    ResetRCnt(0xF2000002U);
                    previousPhase = 0;
                    while (1) {
                        timer = GetRCnt(0xF2000002U);
                        if ((u32)(timer & 0xFFFF) < 0x52B0U) {
                            timerPhase = timer & 0x7F;
                            if (timerPhase < previousPhase) {
                                previousPhase = timerPhase;
                                if (SpuIsTransferCompleted(0) != 0) {
                                    previousPhase = timerPhase;
                                    goto read_header;
                                }
                            }
                            previousPhase = timerPhase;
                        } else {
                            break;
                        }
                    }
                    D_80082808 = 4;
                    D_80082810 = D_80082808;
                    goto check_status;
                }
                goto read_header;
            }
        } else {
        read_header:
            if (CdGetSector(&D_800827F8, 3) == 0) {
                *((volatile u8*)&D_80068B64 + 1) = (u8)(*((volatile u8*)&D_80068B64 + 1) + 1);
                if (D_80082808 == 0) {
                    D_80082808 = 2;
                }
                D_80082810 = D_80082808;
                goto check_status;
            }
            sectorPos = CdPosToInt(&D_800827F8);
            if (sectorPos != CdStream_State.field_2C) {
                if (sectorPos < CdStream_State.field_2C) {
                    if (CdStream_State.field_2C >= (sectorPos + 4)) {
                        goto sector_mismatch;
                    }
                } else {
                sector_mismatch:
                    D_80068B5F += 1;
                    if (D_80082808 == 0) {
                        D_80082808 = 3;
                    }
                    D_80082810 = D_80082808;
                    goto check_status;
                }
            } else {
                CdStream_State.field_2C += 1;
                if ((s16)(u16)CdStream_State.field_4C == 2) {
                    skipIndex = D_80068B78++ & 0xFF;
                    skipStamp = skipIndex | ((CdStream_State.field_1C << 8) & 0xFFFF00);
                    if (D_80068B74 < skipStamp) {
                        if ((((s32 (*)(s32, s32))func_800AF590)(0, 0) << 0x10) == 0) {
                            *(volatile s32*)&D_80068B74 = skipStamp;
                            goto advance_countdown;
                        }
                        goto check_status;
                    }
                    CdGetSector(CdStream_State.sector, 0x200);
                advance_countdown:
                    state                    = &CdStream_State;
                    CdStream_State.countdown = (u16)CdStream_State.countdown - 1;
                    if ((u16)CdStream_State.countdown == 0) {
                        CdStream_State.field_4C = 4;
                    }
                } else {
                    if (CdGetSector(CdStream_State.sector, 0x200) == 0) {
                        *((volatile u8*)&D_80068B64 + 1) = (u8)(*((volatile u8*)&D_80068B64 + 1) + 1);
                        if (D_80082808 == 0) {
                            D_80082808 = 2;
                        }
                        D_80082810 = D_80082808;
                        goto check_status;
                    }
                    if ((u16)CdStream_State.remaining == 0) {
                        if ((CdStream_State.sector->magic & ~0xFF) != 0x4D545300) {
                            D_80082808 = 8;
                            D_80082810 = D_80082808;
                            goto check_status;
                        }
                        if (CdStream_State.sector->field_0 == 0) {
                            CdStream_State.mtsPeriod = (s8)CdStream_State.sector->field_D;
                            CdStream_State.remaining = (s16)(s8)(u8)CdStream_State.mtsPeriod;
                            CdStream_State.mtsParam  = CdStream_State.sector->field_E;
                            if (CdStream_State.sector->field_F & 0x80) {
                                CdStream_State.flags1 |= 4;
                            } else {
                                CdStream_State.flags1 &= 0xFB;
                            }
                            regionState  = &CdStream_State;
                            chunkSectors = 0x30;
                            if ((s8)(u8)regionState->mtsPeriod == 5) {
                                chunkSectors          = 0x18;
                                regionState->ringHalf = 0x2770;
                                if (Display_State.region == 1) {
                                    chunkSectors = 0x14;
                                }
                            } else {
                                regionState->ringHalf = 0x4ED0;
                                if (Display_State.region == 1) {
                                    chunkSectors = 0x28;
                                }
                            }
                            regionState->sectorsPerChunk             = chunkSectors;
                            channels                                 = &CdStream_Channels;
                            channelState                             = (volatile CdStreamState*)channels - 1;
                            *(volatile s32*)&channels->ch[0].spuAddr = channelState->spuBase;
                            /* Keep the channel address stores in initialization order. */
                            SOFT_BARRIER();
                            channelBase             = channelState->spuBase + 0x40;
                            channels->ch[1].spuAddr = channelBase + ((s32)((u16)channelState->ringHalf << 0x10) >> 0xF);
                            channelState->flags1    = (u8)(channelState->flags1 | 1);
                            SOFT_BARRIER();
                            attributes             = channels->ch[0].attr | 0x80;
                            channels->ch[0].attr   = attributes;
                            channels->ch[1].attr   = attributes;
                            channelState->field_1C = (s32)channelState->sector->field_0;
                            channelState->field_38 = (s32)channelState->sector->field_4;
                            if (!((u8)channelState->sector->field_F & 0x60)) {
                                channelState->flags1 = (u8)(channelState->flags1 | 0x40);
                            } else if ((u8)channelState->sector->field_F & 0x40) {
                                channelState->flags1 = (u8)(channelState->flags1 | 0x20);
                            } else {
                                channelState->flags1 = (u8)(channelState->flags1 | 0x10);
                            }
                            goto start_chunk;
                        }
                        if (CdStream_State.sector->field_F & 0x80) {
                            CdStream_State.mtsParam = CdStream_State.sector->field_E;
                            if (CdStream_State.mtsParam != 0) {
                                CdStream_State.flags1 |= 4;
                            } else {
                                CdStream_State.flags1 &= 0xFB;
                            }
                        } else {
                            CdStream_State.flags1  &= 0xFB;
                            CdStream_State.mtsParam = 0;
                        }
                        CdStream_State.remaining = (s16)(s8)(u8)CdStream_State.mtsPeriod;
                    }
                    if (((s16)(u16)CdStream_State.remaining % (s8)(u8)CdStream_State.mtsPeriod) == 0) {
                        CdStream_State.field_1C = CdStream_State.sector->field_0;
                        if (CdStream_State.field_1C == 0) {
                            CdStream_State.field_38 = CdStream_State.sector->field_4;
                        }
                    start_chunk:
                        channelCount = (u8)CdStream_State.sector->magic;
                        /* Preserve the signed comparison of the channel count. */
                        TOUCH_REG(channelCount);
                        if ((channelCount >= 2) && (CdStream_State.sector->field_C == 0)) {
                            CdStream_State.mode      = (s8)(u8)CdStream_State.sector->magic;
                            CdStream_State.remaining = (s8)(u8)CdStream_State.mtsPeriod * (s8)(u8)CdStream_State.mode;
                        }
                        if (CdStream_State.field_1C & 1) {
                            CdStream_State.spuAddr = CdStream_State.spuBase + (s16)(u16)CdStream_State.ringHalf;
                        } else {
                            CdStream_State.spuAddr = CdStream_State.spuBase;
                        }
                        CdStream_State.spuAddr += CdStream_State.sector->field_C * (((s32)((u16)CdStream_State.ringHalf << 0x10) >> 0xF) + 0x40);
                        SpuSetTransferStartAddr((u32)CdStream_State.spuAddr);
                        *(volatile s32*)&D_80068B70 = CdStream_State.spuAddr;
                        SpuWrite((u8*)CdStream_State.sector + 0x10, 0x800U);
                        *(void* volatile*)&D_80068B6C = (u8*)CdStream_State.sector + 0x10;
                        CdStream_State.spuAddr       += 0x7F0;
                    } else {
                        SpuSetTransferStartAddr((u32)CdStream_State.spuAddr);
                        *(volatile s32*)&D_80068B70 = CdStream_State.spuAddr;
                        if (((s16)(u16)CdStream_State.remaining % (s8)(u8)CdStream_State.mtsPeriod) == 1) {
                            writeSize = ((s16)(u16)CdStream_State.ringHalf - 0x7F0) % 0x800;
                            if (writeSize == 0) {
                                writeSize = 0x800;
                            }
                            if (!(CdStream_State.field_1C & 1)) {
                                /* The two ring halves retain separate transfer paths. */
                                SOFT_BARRIER();
                                SpuWrite((u8*)CdStream_State.sector, (writeSize + 0x3F) & ~0x3F);
                                *(void* volatile*)&D_80068B6C = CdStream_State.sector;
                                CdStream_State.spuAddr       += writeSize;
                            } else {
                                SpuWrite((u8*)CdStream_State.sector, (writeSize + 0x3F) & ~0x3F);
                                *(void* volatile*)&D_80068B6C = CdStream_State.sector;
                                CdStream_State.spuAddr       += writeSize;
                            }
                        } else {
                            SpuWrite((u8*)CdStream_State.sector, 0x800U);
                            *(void* volatile*)&D_80068B6C = CdStream_State.sector;
                            CdStream_State.spuAddr       += 0x800;
                        }
                    }
                    state                    = &CdStream_State;
                    CdStream_State.remaining = (u16)CdStream_State.remaining - 1;
                    if ((u16)CdStream_State.remaining == 0) {
                        if (((u8)CdStream_State.flags1 >> 2) & 1) {
                            CdStream_State.countdown = (s16)(s8)CdStream_State.mtsParam;
                            if ((u16)CdStream_State.countdown != 0) {
                                CdStream_State.field_4C = 2;
                                /* This reset is non-volatile and fills the branch delay slot. */
                                *(u16*)&D_80068B78 = 0;
                                goto unlock;
                            } else {
                                goto mark_complete;
                            }
                        } else {
                        mark_complete:
                            state->field_4C = 4;
                        }
                    }
                }
            }
        }
    } else {
        D_80082808 = 7;
        D_80082810 = D_80082808;
    check_status:
        if (*result & 0x10) {
            if (D_80082808 == 0) {
                D_80082808 = 5;
            }
            D_80082810                       = D_80082808;
            CdStream_State.flags            |= 1;
            CdStream_State.field_4C          = 3;
            CdStream_State.flags2           |= 2;
            *((volatile u8*)&D_80068B5C + 1) = (u8)(*((volatile u8*)&D_80068B5C + 1) + 1);
        } else if ((u16)CdStream_State.field_4C != 0) {
            if ((s16)(u16)CdStream_State.field_4C != 4) {
                CdStream_State.field_4C = 3;
                if (D_80082808 == 0) {
                    D_80082808 = 0xB;
                }
                D_80082810             = D_80082808;
                CdStream_State.flags2 |= 2;
            }
        }
    }
unlock:
    D_80068B6A = 0;
}

s32 CdStream_InitDisc(u32* arg0)
{
    struct {
        u8     result[8];
        s8     mode;
        u8     pad[7];
        CdlLOC loc;
    } sp;
    s32          sync;
    u32          flags;
    u32          temp;
    register u32 a asm("v0");
    register u32 b asm("v1");

    flags = *arg0;
    if ((flags >> 1) & 1) {
        temp  = flags & ~2;
        temp  = temp & ~0xFF0;
        *arg0 = temp | 0x10;
    }

    a          = *(volatile u32*)arg0;
    b          = *(volatile u32*)arg0;
    a          = (a >> 4) & 0xFF;
    b          = (b >> 4) & 0xFF;
    D_80068B66 = a;
    switch (b) {
        case 1:
            if (CdControlB(CdlNop, NULL, sp.result) == 0) {
                return 0;
            }
            if (sp.result[0] & CdlStatShellOpen) {
                return 0;
            }
            if (sp.result[0] & CdlStatStandby) {
                *arg0 = (*arg0 & ~0xFF0) | 0x20;
                case 2:
                    if (CdControl(CdlGetTN, NULL, sp.result) != 0) {
                        *arg0 = (*arg0 & ~0xFF0) | 0x40;
                        case 3:
                            sync = CdSync(1, sp.result);
                            if (sync == CdlDiskError) {
                                *arg0 = (*arg0 & ~0xFF0) | 0x20;
                            } else if (sync == CdlComplete) {
                                *arg0 = (*arg0 & ~0xFF0) | 0x40;
                                case 4:
                                    CdIntToPos(0, &sp.loc);
                                    if (CdControl(CdlSeekL, (u8*)&sp.loc, sp.result) != 0) {
                                        *arg0 = (*arg0 & ~0xFF0) | 0x50;
                                        case 5:
                                            sync = CdSync(1, sp.result);
                                            if ((sync == CdlDiskError) && (sp.result[0] & CdlStatError) &&
                                                (sp.result[1] & 0x40)) {
                                                *arg0 = (*arg0 & ~0xFF0) | 0x10;
                                            } else if (sync == CdlComplete) {
                                                *arg0 = (*arg0 & ~0xFF0) | 0x60;
                                                case 6:
                                                    sp.mode = -0x60;
                                                    if (CdControl(CdlSetmode, (u8*)&sp.mode, NULL) != 0) {
                                                        *arg0                        = (*arg0 & ~0xFF0) | 0x70;
                                                        CdStream_State.settleCounter = 0;
                                                    }
                                            }
                                    }
                            }
                    }
            }
            break;
        case 7:
            CdStream_State.settleCounter = CdStream_State.settleCounter + 1;
            if (CdStream_State.settleCounter >= 4) {
                return 1;
            }
            break;
    }
    return 0;
}

void CdReady_InstallCallback(CdlCB arg0)
{
    volatile CdReadyQueue* p;

    p = &CdReady_Queue;
    if (p->callbackInstalled == 0) {
        p->prevCallback = CdReadyCallback(arg0);
    } else {
        CdReadyCallback(arg0);
    }
    CdReady_Queue.callbackInstalled = 1;
}

void CdReady_ClearCallback(void)
{
    volatile CdReadyQueue* p;

    p = &CdReady_Queue;
    if (p->callbackInstalled != 0) {
        CdReadyCallback(0);
        p->prevCallback      = 0;
        p->callbackInstalled = 0;
    }
}

void CdStream_Reset(void)
{
    s32* ptr;
    u32  i;

    ptr = (s32*)&CdReady_Queue;
    for (i = 0; i < 0x16; i++) {
        *ptr++ = 0;
    }

    ptr = (s32*)&CdStream_State;
    for (i = 0; i < 0x36; i++) {
        *ptr++ = 0;
    }

    SetRCnt(RCntCNT2, 0xFFFF, RCntMdNOINTR);
    StartRCnt(RCntCNT2);
    D_80068B58 = 0;
    D_80082808 = 0;
    D_80082810 = D_80082808;
    D_80068B6A = 0;
    D_80068B5C = 0;
}

void CdStream_ArmSpuIrq(void)
{
    volatile CdStreamState* p;

    CdStream_State.flags0 = CdStream_State.flags0 & 0xFD;
    CdStream_State.flags0 = CdStream_State.flags0 & 0xF7;
    p                     = &CdStream_State;
    p->field_4            = 1;
    p->field_18           = 0;
    SpuSetIRQ(0);
    SpuSetIRQCallback(CdStream_SpuIrqHandler);
    SpuSetIRQAddr((p->spuBase + p->ringHalf + 0x4F) & ~0x3F);
    CdStream_State.flags0 = CdStream_State.flags0 & 0xBF;
    CdStream_State.flags0 = CdStream_State.flags0 | 1;
}

void CdStream_SpuIrqHandler(void)
{
    CdStream_State.flags0 = CdStream_State.flags0 | 8;
}

void CdStream_SetPitch(s16 arg0)
{
    CdStreamChannels*       p;
    volatile CdStreamState* q;
    CdStreamChannel*        ch1b;
    CdStreamChannel*        ch1;
    s16                     val;
    s32                     t0;
    s32                     t1;

    p = &CdStream_Channels;
    q = (volatile CdStreamState*)p - 1;

    if ((q->flags0 >> 1) & 1) {
        if (q->flags1 & 1) {
            t0            = p->ch[0].attr;
            t1            = p->ch[1].attr;
            p->ch[0].attr = t0 | 3;
            p->ch[1].attr = t1 | 3;
        } else {
            p->ch[1].attr = 3;
            p->ch[0].attr = 3;
            q->flags1     = q->flags1 | 1;
        }
    }

    if (CdStream_State.flags & 2) {
        ch1b              = &p->ch[1];
        val               = (s16)((arg0 * 0xB5) >> 8);
        ch1b->pitch       = val;
        p->ch[0].pitchAlt = val;
        ch1b->pitchAlt    = val;
        p->ch[0].pitch    = val;
        return;
    }
    ch1               = &p->ch[1];
    ch1->pitchAlt     = arg0;
    p->ch[0].pitch    = arg0;
    ch1->pitch        = 0;
    p->ch[0].pitchAlt = 0;
}

void CdStream_SetFlag14(s32 arg0)
{
    volatile CdStreamState* p;
    u8                      temp;

    p    = &CdStream_State;
    temp = p->flags1;
    if (temp >> 7) {
        p->field_14 = arg0;
        p->flags1   = p->flags1 | 8;
        p->flags0   = p->flags0 | 1;
    }
}

void CdStream_AbortPhase(u32* arg0)
{
    u32 temp_v1;

    temp_v1 = *arg0;
    if ((temp_v1 >> 1) & 1) {
        *arg0 = temp_v1 & ~8;
        return;
    }
    *arg0                   = temp_v1 & ~8;
    CdStream_State.field_4C = 0;
    switch ((*arg0 >> 5) & 0xFF) {
        case 6:
        case 7:
            CdReady_ClearCallback();
            goto shared_flush;
        case 8:
            CdReady_ClearCallback();
            CdFlush();
            CdControlF(CdlPause, NULL);
            *arg0 = ((*arg0 | 8) & ~0x1FE0) | 0x1C0;
            break;
        case 9:
        case 11:
        case 12:
        case 14:
            CdFlush();
            *arg0 = (*arg0 & ~0x1FE0) | 0x1A0;
            /* fallthrough */
        case 13:
            if (SpuIsTransferCompleted(0) == 0) {
                *arg0 |= 8;
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 10:
        shared_flush:
            CdFlush();
            break;
    }
}

void CdStream_FinishQueueEntry(u32* arg0)
{
    CdStream_AbortPhase(arg0);
    if (!((*arg0 >> 3) & 1) && (CdStream_State.doneCb != NULL)) {
        CdStream_State.doneCb(0);
    }
}

void CdReady_Cancel(s16 arg0)
{
    u8            temp;
    s16           idx;
    u32           flags;
    CdReadyEntry* entry;

    temp = CdReady_Queue.locked;
    if (arg0 != 0) {
        idx   = arg0 - 1;
        entry = (CdReadyEntry*)&CdReady_Queue.entries[idx];
        flags = entry->flags;
        if (flags & 1) {
            entry->flags = (flags & ~1) | 4;
        }
        CdReady_Queue.locked = temp;
    }
}

s32 CdStream_IsBusy(void)
{
    if (CdStream_State.flags0 & 1) {
        return 1;
    }
    return (CdReady_Queue.writeIdx != CdReady_Queue.readIdx) ? 1 : (CdStream_State.pending != 0);
}

void CdStream_ClearReadySlot(void)
{
    CdStream_State.readySlot = 0;
}

void CdStream_SetLinkedPitch(s32 arg0)
{
    if ((s8)arg0) {
        CdStream_State.flags = CdStream_State.flags | 2;
    } else {
        CdStream_State.flags = CdStream_State.flags & 0xFD;
    }
}

void CdStream_MarkEnding(void)
{
    CdStream_State.flags   = CdStream_State.flags & 0xFE;
    CdStream_State.flags1  = CdStream_State.flags1 | 2;
    CdStream_State.pending = 0;
}

s32 CdStream_Flush(void)
{
    CdFlush();
    return 0;
}

void CdStream_ConfigureSpuIrq(s32 arg0, u32 arg1)
{
    if (arg0 == 1) {
        if (D_80068B5C != 0) {
            SpuSetIRQ(0);
            SpuSetIRQCallback(0);
        }
        D_80068B5C = arg0;
        SpuSetIRQCallback(CdStream_SpuIrqHandler);
        SpuSetIRQAddr(arg1);
        SpuSetIRQ(1);
    } else if (D_80068B5C != 0) {
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        D_80068B5C = 0;
    }
}

void func_8005BCF8(void)
{
}
