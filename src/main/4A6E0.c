#include "common.h"

#include <psyq/libapi.h>
#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/4A6E0", func_80059EE0);

INCLUDE_ASM("main/nonmatchings/4A6E0", func_8005A94C);

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
    CdStreamChannels*         p;
    volatile CdStreamState*   q;
    CdStreamChannel*          ch1b;
    register CdStreamChannel* ch1 asm("v0");
    s16                       val;
    s32                       t0;
    s32                       t1;

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

void func_8005B920(s32 arg0)
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
