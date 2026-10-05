#ifndef MAIN_PRIVATE_STREAM_H
#define MAIN_PRIVATE_STREAM_H

#include "types.h"

extern u16 D_8006AC58;

void Mdec_BeginDecode(void* arg0);

/// Requests a rebuild of the VLC lookup table in the shared image workspace.
///
/// Call when a background load reuses the image storage. Sets a pending flag
/// without building the table or accessing the buffer; repeated requests coalesce.
/// `CdCmd_StepVlcRebuild` rebuilds it before the next standalone image decode,
/// once no scene payload is available. The image workspace must then be writable.
/// A scene decode using that workspace can also satisfy the request.
void mdecRequestImageVlcRebuild(void);

/// Initializes a stream slot and its display buffers before playback.
u32 Stream_InitializePlayback(u32 slotIndex);

/// Uploads and presents a completed streaming frame when its timing allows.
void Stream_PresentFrame(void);

/// Returns 1 if any resident movie with an ID below 100 has a loaded sector, else 0.
///
/// `unusedLocation` is ignored: this examines the entire table, without a room,
/// sub-ID or `viewStream` filter. The argument is retained by the resident ABI.
s16 streamHasLoadedViewMovie(void* unusedLocation);

#endif // MAIN_PRIVATE_STREAM_H
