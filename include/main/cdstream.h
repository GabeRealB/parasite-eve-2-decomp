#ifndef CDSTREAM_H
#define CDSTREAM_H

#include "common.h"

#include <psyq/libcd.h>

// Types — CD ready queue + CD→SPU MTS stream

/// Sector payload pointed to by CdStreamState::sector (MTS audio stream sector).
typedef struct _MtsSector {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ u32 magic; // high 3 bytes = "MTS", low byte = channel count
    /* 0x0C */ s8  field_C;
    /* 0x0D */ u8  field_D;
    /* 0x0E */ u8  field_E;
    /* 0x0F */ s8  field_F;
} MtsSector;

/// BSS object CdStream_State (size 0x58). CD/SPU stream runtime state.
typedef struct _CdStreamState {
    /* 0x00 */ u8         flags0; // bit0 busy, bit1 voice-on, bit3 IRQ, bit4 voices alloc, bit6 stop
    /* 0x01 */ u8         flags1; // bit0 voices started, bit1 ending, bit3 param pending, bit7 enable param
    /* 0x02 */ u8         flags2; // bit0 continue-arm, bit1/2 stream phase, bit3 XA pause
    /* 0x03 */ u8         phase;  // 1 = completing, 2 = streaming
    /* 0x04 */ u8         field_4;
    /* 0x05 */ u8         pad_5;
    /* 0x06 */ s16        readySlot; // 1-based CdReady_Queue index, 0 = none
    /* 0x08 */ void       (*doneCb)(s32);
    /* 0x0C */ void       (*startCb)(s32);
    /* 0x10 */ void       (*voiceFreeCb)(s32);
    /* 0x14 */ s32        field_14;
    /* 0x18 */ s32        field_18;
    /* 0x1C */ s32        field_1C;
    /* 0x20 */ s32        field_20;
    /* 0x24 */ s32        spuAddr;
    /* 0x28 */ s32        startSector;
    /* 0x2C */ s32        field_2C;
    /* 0x30 */ s32        field_30;
    /* 0x34 */ s32        field_34;
    /* 0x38 */ s32        field_38;
    /* 0x3C */ s32        spuBase;
    /* 0x40 */ s16        sectorsPerChunk; // 0x18 (NTSC) or 0x14 (PAL)
    /* 0x42 */ s16        ringHalf;        // 0x2770; half ring used in SPU addr math
    /* 0x44 */ s16        countdown;
    /* 0x46 */ s8         mtsPeriod;       // divisor for remaining % period SpuWrite cadence
    /* 0x47 */ u8         mtsParam;
    /* 0x48 */ MtsSector* sector;
    /* 0x4C */ s16        field_4C;
    /* 0x4E */ s16        remaining;
    /* 0x50 */ u8         voiceL;
    /* 0x51 */ u8         voiceR;
    /* 0x52 */ s8         mode;
    /* 0x53 */ u8         flags;         // bit1 = linked L/R pitch
    /* 0x54 */ u16        pending;
    /* 0x56 */ u16        settleCounter; // disc init settle ticks
} CdStreamState;
STATIC_ASSERT_SIZEOF(CdStreamState, 0x58);

/// One half of CdStream_Channels (stride 0x40). Dual L/R audio channel state.
/// Immediately follows CdStream_State in BSS (CdStream_Start treats it as
/// (CdStreamChannel*)(&CdStream_State + 1)).
typedef struct _CdStreamChannel {
    /* 0x00 */ s32  voiceMask;
    /* 0x04 */ s32  attr; // Spu voice attribute bitfield (key-on bits etc.)
    /* 0x08 */ s16  pitch;
    /* 0x0A */ s16  pitchAlt;
    /* 0x0C */ s16  field_C;
    /* 0x0E */ s16  field_E;
    /* 0x10 */ byte pad_10[4];
    /* 0x14 */ s16  field_14; // often 0x1000
    /* 0x16 */ byte pad_16[6];
    /* 0x1C */ s32  spuAddr;
    /* 0x20 */ s32  spuAddr2;
    /* 0x24 */ byte pad_24[0x16];
    /* 0x3A */ s16  field_3A;
    /* 0x3C */ s16  field_3C;
    /* 0x3E */ byte pad_3E[2];
} CdStreamChannel;
STATIC_ASSERT_SIZEOF(CdStreamChannel, 0x40);

/// BSS object CdStream_Channels (size 0x80). Two CdStreamChannel channels at +0x00 / +0x40.
typedef struct _CdStreamChannels {
    /* 0x00 */ CdStreamChannel ch[2];
} CdStreamChannels;
STATIC_ASSERT_SIZEOF(CdStreamChannels, 0x80);

/// One slot in CdReady_Queue.entries (stride 0x14).
/// flags: bit0 active, bit1 armed, bit2 cancel/pending, bit3 result.
typedef struct _CdReadyEntry {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 sectorPos;
    /* 0x08 */ s32 pollFn;  // s32 (*)(CdReadyEntry*)
    /* 0x0C */ s32 doneFn;  // void (*)(void) when complete
    /* 0x10 */ s32 errorFn; // void (*)(CdReadyEntry*) on cancel path
} CdReadyEntry;
STATIC_ASSERT_SIZEOF(CdReadyEntry, 0x14);

/// BSS object CdReady_Queue (size 0x58). Ring of CD ready work items + callback state.
typedef struct _CdReadyQueue {
    /* 0x00 */ u8           locked;            // re-entrancy guard
    /* 0x01 */ u8           callbackInstalled; // CdReadyCallback currently ours
    /* 0x02 */ u8           readIdx;
    /* 0x03 */ u8           writeIdx;
    /* 0x04 */ void*        prevCallback; // previous CdReadyCallback
    /* 0x08 */ CdReadyEntry entries[4];
} CdReadyQueue;
STATIC_ASSERT_SIZEOF(CdReadyQueue, 0x58);

/// BSS object CdStream_Params (size 0x20). CD/SPU stream setup block for
/// func_800567E4 / CdStream_Start: sector position, buffer, callbacks, and
/// voice indices.
typedef struct _CdStreamParams {
    /* 0x00 */ s32   startSector;
    /* 0x04 */ s32   spuBase;
    /* 0x08 */ void* sectorBuf;
    /* 0x0C */ void  (*doneCb)(void);
    /* 0x10 */ s32   startCb;     // stored into CdStreamState::startCb
    /* 0x14 */ s32   voiceFreeCb; // stored into CdStreamState::voiceFreeCb
    /* 0x18 */ s16   pitch;
    /* 0x1A */ s8    voiceL;
    /* 0x1B */ s8    voiceR;
    /* 0x1C */ u8    mode;
    /* 0x1D */ u8    pad_1D[3];
} CdStreamParams;
STATIC_ASSERT_SIZEOF(CdStreamParams, 0x20);

// CD ready work queue
void CdReady_InstallCallback(CdlCB arg0);
void CdReady_ClearCallback(void);
s32  CdReady_Enqueue(CdReadyEntry* arg0);
void CdReady_Poll(void);
void CdReady_Cancel(s16 slot /* 1-based */);

// CD → SPU MTS stream
s32  CdStream_InitDisc(u32* phaseFlags);
void CdStream_Reset(void);
void CdStream_Start(CdStreamParams* params);
void CdStream_Continue(void);
void CdStream_Stop(void);
void CdStream_ArmSpuIrq(void);
void CdStream_SpuIrqHandler(void);
void CdStream_SetPitch(s16 pitch);
void CdStream_AbortPhase(u32* phaseFlags);
void CdStream_FinishQueueEntry(u32* phaseFlags);
s32  CdStream_IsBusy(void);
void CdStream_ClearReadySlot(void);
void CdStream_SetLinkedPitch(s32 enabled);
void CdStream_MarkEnding(void);
s32  CdStream_Flush(void);
void CdStream_ConfigureSpuIrq(s32 enable, u32 irqAddr);

// Globals in unknown_syms.h: CdStream_State, CdStream_Channels, CdReady_Queue, CdStream_Params

// --- APIs (from unknown_syms) ---
void CdStream_Drive(void);
void CdStream_ReadyMts(s32 arg0, u8* arg1);

#endif // CDSTREAM_H
