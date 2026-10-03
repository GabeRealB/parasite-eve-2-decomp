#ifndef MAIN_STREAM_H
#define MAIN_STREAM_H

#include <psyq/sys/types.h>

#include "types.h"

#include "main/stream_types.h"

extern StreamSlot Stream_Slots[15];

extern u16 D_8005EAEC;

extern u16 D_8005EAEE;

extern StreamInterFile D_8006AC30;

extern u16 D_8006AC3C;

extern void* D_8006AC40;

extern void* D_8006AC44;

extern u_long* D_8006AC48[];

extern u_long* D_8006AC50[];

extern u16 D_8006AC5A;

extern u16 D_8006AC5C;

extern u16* D_8006AC60;

extern u16 D_8006AC6C;

void Mdec_ResolveStreamBuffer(u8* arg0);

u16 Stream_RestoreAfterLoad(s32 arg0, s32 arg1);

void Stream_ResetRestoreState(void);

s16 Stream_FindSlot(u8* arg0, s32 arg1, s32 arg2);

s16 Stream_FindSlotByKey(u8* arg0);

StreamSlot* Stream_GetSlot(u32 arg0);

u16 Stream_GetSlotField1A(u32 arg0);

void Stream_KickDecode(u32 arg0);

/// Advances the CD/MDEC playback state machine.
extern s32 Stream_PollPlayback(u16 resume, s32 sectorOffset);

#endif // MAIN_STREAM_H
