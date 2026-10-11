#ifndef MAIN_STREAM_H
#define MAIN_STREAM_H

#include <psyq/sys/types.h>

#include "types.h"

#include "main/session_types.h"
#include "main/stream_types.h"

/// Resident stream descriptors for the current folder or title movie selection.
///
/// Fifteen directory-order slots hold absolute start sectors; zero startSector
/// marks an unavailable slot. Folder import copies up to its zero-key terminator
/// without checking capacity, so input must contain at most fifteen records.
/// Folder preparation invalidates start sectors only; title copies ten STAGE0
/// descriptors into the prefix. Playback borrows records until the next import,
/// invalidation or whole-table clear; this table owns no payload buffers.
extern StreamSlot gStreamSlots[15];

/// Expanded-input slot (0 or 1) used for the next movie VLC expansion/MDEC submission.
///
/// Playback reset selects zero; submitting a frame toggles it. The selected
/// buffer must hold the complete expanded command stream before submission.
extern u16 gMdecMovieInputSlot;

/// Completed movie-output slot (0 or 1); the next MDEC transfer targets its opposite.
///
/// Output completion advances it once per display column or texture frame.
/// Texture presentation reads the selected slot; configured slots may alias.
extern u16 gMdecMovieOutputSlot;

/// Disc movie-container location populated by the ISO root scan.
///
/// The last root `.STR` entry supplies the absolute sector; zero means absent.
/// A root rescan replaces it. Nonzero movie volume selections seek within it.
extern StreamInterFile gStreamInterFile;

/// Nonzero permits texture-movie shutdown to release the game-pause block.
///
/// Buffer assignment enables this by default; Acropolis Plaza keeps the block.
extern u16 gStreamMovieReleasePauseOnStop;

/// Active borrowed movie workspace, with a byte extent supplied by its allocation.
///
/// Selected from a retained CD-command reservation or a temporary movie heap.
/// Layouts vary with stage/area, format and dimensions; this is raw storage,
/// not one fixed record. Word-aligned table, ring, expanded-input and pixel
/// intervals must fit their selected arenas and stay live until decoding and
/// GPU/DMA transfers end. Heap restoration/reinitialization invalidates them.
extern void* gStreamMovieWorkspace;

/// Byte address just after the final reserved buffer in the display/map layout.
///
/// Display layouts end after input slot 1; map layouts after output slot 1.
/// Stage-zero texture placement leaves it unchanged. This retained metadata has
/// no reader and does not describe an allocation's full capacity.
extern u8* gStreamMovieBufferLayoutEnd;

/// Two borrowed word-aligned movie output slots, which may share storage.
///
/// Texture output is a full RGB16 frame (width*height*2 bytes), stored as
/// successive full-height 16-pixel columns. Display output is one such column:
/// 16*height*2 bytes for RGB16, or 16*height*3 for RGB24. Pointers use the SDK's
/// 32-bit DMA/GPU word interface; byte casts advance through pixel storage.
/// Each selected interval must fit its arena and outlive pending transfers.
extern u_long* gMdecMovieOutputBuffers[2];

/// Two borrowed word-aligned slots for VLC-expanded movie MDEC commands.
///
/// Capacities depend on the active resident/map layout; each expanded frame
/// must fit its reserved interval. These contain commands, not decoded pixels.
/// An expanded payload of N words requires (N+1)*4 bytes including its command
/// word; N is the compressed frame header's low halfword.
/// Keep each slot intact until MDEC has consumed its submitted input.
extern u_long* gMdecMovieExpandedBuffers[2];

/// Cached descriptor width in pixels, shared with map buffer-placement hooks.
///
/// Must be a positive multiple of 16 for the movie's complete-column transfers;
/// width/height products and their byte reservations must fit the selected arenas.
extern u16 gStreamMovieWidthPixels;

/// Nonzero lets the movie run without holding game pause or the CD command queue busy.
///
/// Skips the initial pause block and clears CD busy after the first read starts.
/// Buffer assignment clears it; Fountain and Eve Elevator enable it. Independent
/// of VRAM staging, which can also be used by blocking movie playback.
extern u16 gStreamMovieBackgroundPlayback;

/// Borrowed word-aligned STR ring (32 sectors for texture, 40 for display movies).
///
/// Each sector reserves 2048 bytes. Texture storage follows the VLC table in
/// actor buffer zero; display storage follows it in the active movie workspace.
/// The ring owns acquired compressed frames until expansion releases them;
/// stop ring delivery and pending decoding before repurposing the arena.
extern u_long* gStreamMovieRingBuffer;

/// Cached descriptor height in rows, shared with map buffer-placement hooks.
///
/// Full-frame and column capacities derive from this height and pixel width;
/// the active buffers and VRAM destinations must fit every complete transfer.
extern u16 gStreamMovieHeightRows;

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

/// Advances the pending standalone or cached-scene background image decode.
///
/// With no scene payload, rebuilds a requested VLC table before expanding a
/// standalone bitstream. An available scene payload selects the scene decoder,
/// which also uploads its optional images and copies timing data. Repeated calls
/// service the request; MDEC output completes through its DMA callback. Scene
/// header/output waits give up after 91 polls and report completion as well.
/// Input, VLC, payload and output storage must remain valid until consumed, and
/// the selected auxiliary region must fit the complete expanded command stream.
/// This call neither allocates a request nor guarantees completion on return.
void mdecStepImageDecode(void);

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

/// Resets a movie's playback state and assigns its retained decode/display buffers.
///
/// The low halfword of `slotIndex` must select a loaded movie slot (0..14).
/// Requires reserved movie/actor workspaces and serialized decoder/CD use;
/// an earlier movie's DMA and STR ring must be stopped before reinitializing.
/// Display movies also clear/configure both framebuffers and build the VLC table.
/// This does not seek or start decoding. A missing INTER file is ignored here;
/// the slot state has already been reset but buffer/display setup is incomplete.
void streamInitMoviePlayback(u32 slotIndex);

/// Polls movie startup, decoding, pause, looping and disc-recovery playback.
///
/// Initialize the selected movie and its retained workspaces before the first
/// poll. With `sectorIsAbsolute` zero, `sectorOffset` counts sectors from the
/// movie start; otherwise it is an absolute CD sector. Keep these inputs stable
/// while the initial seek is pending. Recovery resumes at the last published
/// frame using ten sectors per frame, independently of the initial offset.
/// Returns 0 while running or waiting, and 1 after a nonlooping movie finishes.
/// Completion restores display ownership but retains movie allocations.
/// Requires serialized use of the resident CD channel; read-start retries and
/// disc recovery have no timeout here.
extern s32 streamPollMoviePlayback(u16 sectorIsAbsolute, s32 sectorOffset);

#endif // MAIN_STREAM_H
