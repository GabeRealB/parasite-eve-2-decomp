#ifndef CDSTREAM_H
#define CDSTREAM_H

#include "common.h"

#include <psyq/libcd.h>

// Types live in game.h (CdStreamState, CdReadyQueue, MtsSector, …) until the
// kitchen-sink header is fully split. This module header owns the public API.

// =============================================================================
// CD ready work queue (CdReady_Queue)
// =============================================================================

void CdReady_InstallCallback(CdlCB arg0);
void CdReady_ClearCallback(void);
s32  CdReady_Enqueue(struct _CdReadyEntry* arg0);
void CdReady_Poll(void);
void CdReady_Cancel(s16 slot /* 1-based */);

// =============================================================================
// CD → SPU MTS stream (CdStream_State / CdStream_Channels)
// =============================================================================

s32  CdStream_InitDisc(u32* phaseFlags);
void CdStream_Reset(void);
void CdStream_Start(struct _CdStreamParams* params);
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

// Globals (BSS) are declared in unknown_syms.h until they gain a typed home:
//   CdStream_State    @ 0x80082818
//   CdStream_Channels @ 0x80082870
//   CdReady_Queue     @ 0x800828F0
//   CdStream_Params   @ 0x800827C4

#endif // CDSTREAM_H
