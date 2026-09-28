#ifndef MAIN_PRIVATE_STREAM_H
#define MAIN_PRIVATE_STREAM_H

#include "types.h"

extern u16 D_8006AC58;

void Mdec_BeginDecode(void* arg0);

/// Initializes a stream slot and its display buffers before playback.
u32 Stream_InitializePlayback(u32 slotIndex);

/// Uploads and presents a completed streaming frame when its timing allows.
void Stream_PresentFrame(void);

s16 Stream_HasActiveLowId(void* unused);

#endif // MAIN_PRIVATE_STREAM_H
