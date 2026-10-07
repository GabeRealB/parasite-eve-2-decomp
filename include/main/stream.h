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

/// Reserves movie decoding storage and optionally saves the VRAM images it displaces.
///
/// Resets GPU drawing and invalidates model buffers before resetting the saved
/// whole image-memory region as an auxiliary heap. Reserves 0x4A800 bytes in
/// normal video mode, otherwise 0x45400 bytes, without checking allocation failure.
/// The saved region must be configured and large enough for this allocation and
/// heap metadata; its previous allocations must no longer be in use.
///
/// A nonzero `preserveVramImages` saves the two 160-word by 256-row regions at
/// (320,0) and (320,256) to (704,0) and (864,0). Keep those backup regions intact
/// through `streamPollGameRestore`. Blocks game pause until restoration releases
/// it; the workspace remains borrowed by the movie decoder until restoration.
void streamPrepareMovieWorkspace(s16 preserveVramImages);

/// Restores game image memory and model buffers after movie playback has ended.
///
/// Call `streamResetGameRestore` before beginning a new restore. The first poll
/// restores any saved VRAM images and configures memory for the current stage
/// and area. Only low halfwords are examined: `selectConfiguredAuxHeap == 1`
/// selects the saved auxiliary portion before restoring model buffers; every
/// other value keeps the freshly configured selection. A nonzero
/// `reloadSpriteImages` queues an images-only load for the current sprite variant.
/// The session, map resources and any saved VRAM regions must remain available.
///
/// Returns 0 while that load is pending, then 1 once the CD queue is idle and
/// the pause block is released. Without a reload, the first poll returns 1 with
/// the pause block intact; a later poll can release it once the queue is idle.
/// Completed polls keep returning 1. Neither result restores display visibility.
u16 streamPollGameRestore(s32 selectConfiguredAuxHeap, s32 reloadSpriteImages);

/// Makes the next `streamPollGameRestore` begin a new post-movie restoration.
///
/// Only resets the polling step; it does not restore memory, discard the saved
/// VRAM images or release the pause block. Any preceding restore must have ended.
void streamResetGameRestore(void);

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
