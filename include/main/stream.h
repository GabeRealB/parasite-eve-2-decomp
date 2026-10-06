#ifndef MAIN_STREAM_H
#define MAIN_STREAM_H

#include <psyq/sys/types.h>

#include "types.h"

#include "main/session_types.h"
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

/// Schedules the cached scene image selected by the byte at `viewId`.
///
/// Reads this byte only during the call. A matching, fully loaded header selects
/// decode, actor 0/1/2 or external payload storage; shared actor storage follows
/// its reserved VLC-table and timing prefixes. Header selectors and byte offsets
/// must be valid for the loaded scene, and its payload and decode workspaces must
/// remain available through completion. No allocation or capacity check occurs.
///
/// If the header is absent or a payload is still loading, the decoder waits and
/// retries using the current session's view. This replaces the pending request;
/// it does not queue an independent decode or retain the `viewId` pointer.
void mdecRequestSceneImageDecode(const u8* viewId);

u16 Stream_RestoreAfterLoad(s32 arg0, s32 arg1);

void Stream_ResetRestoreState(void);

/// Failure result from the movie-slot lookups.
enum { STREAM_SLOT_NOT_FOUND = -1 };

/// Finds a loaded movie by stream ID, room and sub-ID, returning 0..14 or `STREAM_SLOT_NOT_FOUND`.
///
/// Borrows `location` for this call and reads only `view` (the stream ID) and
/// `room`. A descriptor group of `STREAM_KEY_ANY_GROUP` matches any room.
/// Only the low 16 bits of `subId` and `requireViewStream` are used; a nonzero
/// filter requires the descriptor's `viewStream` to be nonzero. A matching
/// room-specific descriptor rejected by that filter ends the search; a rejected
/// wildcard descriptor permits later candidates. No playback state is changed.
s16 streamFindMovieSlot(const GameLocationKey* location, s32 subId, s32 requireViewStream);

/// Finds a view-enabled movie by stream ID and room, returning 0..14 or `STREAM_SLOT_NOT_FOUND`.
///
/// Borrows `location` for this call and reads only `view` and `room`, with
/// `STREAM_KEY_ANY_GROUP` matching any room. Sub-ID and `startSector` are not
/// checked, so this can read descriptor metadata without a loaded movie sector.
s16 streamFindViewMovieSlot(const GameLocationKey* location);

/// Borrows a writable resident stream descriptor at `slotIndex` (0..14).
///
/// The caller must supply a valid index, never `STREAM_SLOT_NOT_FOUND`; there
/// is no bounds or kind check. The pointer remains valid, but its descriptor
/// can be cleared or replaced when stream tables are reloaded.
StreamSlot* streamGetSlot(u16 slotIndex);

/// Reads a movie slot's playback stop frame, numbered from 1.
///
/// `slotIndex` must be 0..14 and identify a movie descriptor. No bounds, kind
/// or loaded-sector check is performed; the limit can precede the STR's end.
u16 streamGetFrameLimit(u16 slotIndex);

void Stream_KickDecode(u32 arg0);

/// Advances the CD/MDEC playback state machine.
extern s32 Stream_PollPlayback(u16 resume, s32 sectorOffset);

#endif // MAIN_STREAM_H
