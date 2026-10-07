#ifndef MAIN_FS_H
#define MAIN_FS_H

#include "types.h"

#include "main/fs_types.h"
#include "main/session_types.h"
#include "main/stream_types.h"

/// The program's one resident CD-command queue.
///
/// Main and the overlays that load files, play movies or run scene audio share
/// this object for the executable's lifetime. Callers borrow it, and a full
/// clear zeroes it in place rather than allocating another.
extern CdCmdQueue gCdCmdQueue;

extern u8 Fs_CdOpStatus;

extern StreamSlot Fs_Streams[0xa];

// Boot load-buffer pointer (data unit `boot_loadbuf`)
extern FsImgBuffers* Fs_ImgBuffers;

/// Bytes produced in each streaming destination; -1 means pending.
extern s32 Fs_ChunkOutputSizes[3];

extern FsResourceSlot D_8006C338[50];

extern s8 D5B498_8006C234;

/// Bases of three fixed 0x18000-byte work buffers. The movie player takes one
/// as its VLC table and decode area, chunked file loads stream into them, and
/// overlays borrow them for primitives and saved state, so the bytes have no
/// single type and each user views them as its own.
extern void* Fs_ActorLoadBase0;

extern void* Fs_ActorLoadBase1;

extern void* Fs_ActorLoadBase2;

/// Where a development unit holds the replay that `DISPLAY_DEMO_FIXED_REPLAY`
/// plays, in memory a retail console does not have: the same save state and
/// input record a demo otherwise reads from `Fs_ActorLoadBase2`.
#define FILE_SYSTEM_FIXED_REPLAY_BASE ((u8*)0x80600100)

/// Starts STR playback; the first command-argument byte is a stream slot (0..14).
enum { CD_COMMAND_PLAY_STREAM = 0x61 };

/// Copies a request into the CD ring and returns its slot index (0..7).
///
/// Only the low byte of `command` is stored. `fileKey` supplies bytes 3
/// (CDF stage), 2 (file group) and 0 (file index); byte 1 is ignored.
/// `commandArgs` supplies four bytes interpreted by the opcode, as in
/// `CdCmdEntry.args`. Both sources are read immediately and are not retained.
/// They are raw byte addresses: signed-byte arrays and serialized keys are
/// accepted without changing their representation or requiring alignment.
/// A zero address still supplies bytes from low RAM; it does not omit a block.
/// All seven source-byte reads occur even if the opcode ignores those fields.
/// Producers must keep the next write index from overtaking the read index;
/// the ring has eight slots and this routine does not test whether it is full.
s32 cdCmdEnqueue(s32 command, const void* fileKey, const void* commandArgs);

/// Saves a deferred replacement request without advancing the CD ring.
///
/// Uses the byte-source contract of `cdCmdEnqueue`, overwrites the previous
/// replacement, and retains no source pointers. `cdCmdCommitReplacement` later
/// appends it to the ring; a zero command marks the replacement empty.
void cdCmdStageReplacement(s32 command, const void* fileKey, const void* commandArgs);

/// Appends the deferred replacement to the CD ring and retires its staged opcode.
///
/// Returns the written slot (0..7), or -1 when no replacement is staged. Copies
/// all eight request bytes, including opcode-unused arguments. Requires the free
/// ring capacity of `cdCmdEnqueue`; the saved active request is unchanged.
s16 cdCmdCommitReplacement(void);

/// Discards requests after the ring head, preserving that head and its progress.
///
/// Returns 1 if the ring was empty, otherwise 0 even when only the head remains.
/// Only discarded opcodes are cleared; the slots' other bytes remain intact.
s32 cdCmdDropQueuedTail(void);

/// Returns 1 when normal dispatch is selected and the request ring is empty.
///
/// Cancellation or suspension returns 0 even with an empty ring. This does not
/// test the display-busy latch, drive status or saved scene-audio request.
u16 cdCmdIsIdle(void);

/// Returns 1 when a CD-ring slot has an empty or retired opcode, otherwise 0.
///
/// `slot` must be in 0..7, normally a value returned by an enqueue. This tests
/// only that slot's opcode; it does not test queue or drive idleness. Poll before
/// another producer reuses the slot, since slot indices carry no generation.
u16 cdCmdIsSlotEmpty(s16 slot);

/// Marks a blocking CD operation busy and latches the display's CD-busy state.
///
/// The display latch is written only when the queue changes from idle to busy.
/// This flag is independent of the ring/dispatch test in `cdCmdIsIdle`.
void cdCmdSetBusy(void);

/// Starts the shared request iterator at the current ring head.
///
/// Includes the head and every queued tail entry up to the current write index.
/// There is one iterator for all callers; finish a walk before starting another.
void cdCmdResetEntryIterator(void);

/// Borrows the next ring entry, or returns `NULL` at the current write index.
///
/// Call `cdCmdResetEntryIterator` before walking. Entries are returned in queue
/// order, including the head. Queue writes/retirement can change the walk and
/// invalidate a borrowed entry's contents; copy it before modifying the queue.
CdCmdEntry* cdCmdNextQueuedEntry(void);

/// Saves all eight bytes of the ring head in the active-request snapshot.
///
/// Does not advance the ring or alter the saved resume sector or dispatch
/// phase. The saved bytes survive retirement or reuse of the ring slot.
void cdCmdSaveHeadRequest(void);

/// Clears the current handler's progress and retires the head of a nonempty ring.
///
/// Releases the queue/display busy latch and resets the handler/cancel steps,
/// play-clock pause and pending drive operation. An empty ring still receives
/// those resets. The saved active request and replacement request are retained.
void cdCmdCompleteHeadRequest(void);

/// Requests cancellation of the ring head or the retained scene/audio session.
///
/// Saves all eight head bytes when its opcode is nonzero, then selects the
/// cancellation phase. With an empty head, active scene/audio mode selects that
/// phase using the existing snapshot. Returns 1 when cancellation is requested,
/// or 0 when neither is present; completion requires subsequent CD dispatches.
s32 cdCmdRequestCancel(void);

s32 CdCmd_PollStatus(s32 arg0, s32 arg1);

/// Enqueues playback of the selected scene/audio session.
///
/// A valid scene slot queues `CD_COMMAND_PLAY_SCENE_AUDIO` and marks audio as
/// starting. Without a selected slot, it enters scene-playing mode immediately.
/// The selected descriptor and prepared playback buffers must survive the
/// request. Uses `cdCmdEnqueue`'s ring-capacity contract.
void cdCmdEnqueueScenePlayback(void);

/// Enqueues audio start for the selected scene/audio session.
///
/// A selected slot queues `CD_COMMAND_START_SCENE_AUDIO` and marks audio as
/// starting; no slot leaves the queue and mode unchanged. Its handler retires
/// the request when audio has started, retaining the session for later playback.
/// Selection and prepared buffers must survive consumption. Uses `cdCmdEnqueue`'s
/// free-ring-capacity contract.
void cdCmdEnqueueSceneAudioStart(void);

/// Stages a deferred audio-start request for the selected scene slot.
///
/// A valid slot replaces the deferred request with `CD_COMMAND_START_SCENE_AUDIO`;
/// no slot leaves the previous replacement intact. `cdCmdCommitReplacement` later
/// enqueues it. Its handler retires the request once audio has started, retaining
/// the scene session for a later playback request. Selection/buffers must remain
/// valid through consumption; this routine does not change scene/audio mode.
void cdCmdStageSceneAudioStart(void);

/// Empty entry point in the caption/scene-control handshake; its intended role is unproven.
void cdCmdSceneControlNoOp(void);

/// Discards the deferred replacement, requests CD cancellation and finishes the scene.
///
/// Cancellation targets the ring head or retained scene/audio session and
/// completes through subsequent CD dispatches. Scene streaming flags and saved
/// random state are restored immediately. Requires a prior successful scene
/// selection; buffer and task teardown remain with their owners.
void cdCmdCancelScene(void);

/// Reserves scene VLC/timing/decode storage and the current movie workspace.
///
/// A selected scene borrows actor storage or allocates from the initialized
/// auxiliary heap according to its descriptor. Matching actor VLC/timing
/// selectors put timing words after a `STREAM_VLC_TABLE_BYTES` prefix, including
/// when the VLC mode selects image storage. Borrowed actor contents must be
/// expendable, and timing/decode payloads must fit their buffers.
/// The movie workspace is allocated after scene storage, before model buffers:
/// stage zero reserves a fixed title workspace; other stages require a matching
/// movie and a nonzero live-save stage/area size-table entry. Returns that raw
/// workspace, or `NULL` when none is reserved or its allocation fails. Scene
/// reservations may already have occurred when the movie workspace is `NULL`.
/// Previous playback storage must have been retired before reserving again;
/// auxiliary allocations last until that heap is released or reinitialized.
void* cdCmdReservePlaybackBuffers(void);

/// Resets the movie frame to 1 and prepares actor buffer 0 for loaded view movies.
///
/// Builds its VLC table when any loaded movie has a view ID below 100; the
/// current view does not constrain that test. It also clears the session field
/// associated with actor buffer 0, whose nonzero meaning remains unproven.
/// The actor buffer must be available for overwrite; no storage is allocated.
void cdCmdPrepareViewMovie(void);

/// Publishes the reserved movie workspace for the current location's movie.
///
/// A sub-ID-zero movie match selects the workspace previously returned by
/// `cdCmdReservePlaybackBuffers`; no match keeps the existing stream workspace.
/// Always clears the decoded-frame availability flag. It neither allocates nor
/// initializes a decoder; the selected workspace must survive movie playback.
void cdCmdSelectMovieWorkspace(void);

/// Selects a scene/audio descriptor for subsequent buffer setup and playback.
///
/// Matches the three exact 16-bit key values and a secondary sub-ID of zero.
/// Group 0 searches stage-zero descriptors, otherwise the loaded stage table.
/// Saves the selected slot, or `CD_COMMAND_NO_SCENE_SLOT` on failure. Successful
/// selection borrows the descriptor and saves/seeds random state; keep the table
/// live until the scene finishes, and finish an earlier scene before selecting
/// another. Buffer allocation and request staging/enqueue happen separately.
void cdCmdSelectScene(u16 group, u16 streamId, u16 subId);

/// Display-resource profiles accepted by `cdCmdEnqueueDisplayResource`.
enum {
    CD_COMMAND_DISPLAY_LOAD_MENU              = 0, // Relocate images: X offset -8 pages, header Y adjustment -3
    CD_COMMAND_DISPLAY_LOAD_PREVIEW           = 1, // Normal load: X unchanged, header Y adjustment -2
    CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW = 2, // Relocate images: X unchanged, header Y adjustment -2
    CD_COMMAND_DISPLAY_LOAD_DEFAULT           = 3, // Normal load without image offsets
    CD_COMMAND_DISPLAY_LOAD_SEEK_CURRENT_VIEW = 4, // Consume a pending seek back to the mapped room view
};

/// Queues a stage-zero display resource or a conditional seek to the current view.
///
/// Uses the low bytes of both ID components: the packed file ID is
/// `20000 + fileIdHundreds * 100 + fileIndex`. The low byte of `loadProfile`
/// must select one of `CD_COMMAND_DISPLAY_LOAD_*`; other values leave load
/// options uninitialized. Image X offsets count 64-word VRAM pages. Header Y
/// adjustments count rows and apply to headers at rows 245..255, in addition
/// to the load policy's own displacement (one extra header row for relocation).
/// The seek profile ignores the supplied ID and uses the live stage, area,
/// mapped view and sprite variant. It consumes the pending view-seek latch even
/// when a scene-audio ring head suppresses the enqueue. Other profiles arm it.
/// Requires an initialized scratch stack with eight free bytes and the free
/// ring capacity of `cdCmdEnqueue`. A pending view seek also requires loaded
/// gameplay view mappings. Neither parameter block survives this call.
void cdCmdEnqueueDisplayResource(s32 fileIdHundreds, s32 fileIndex, s32 loadProfile);

void CdCmd_StepVlcRebuild(void);

void Fs_ReadSectorEx(s32 sector, s32 endSector, u8* dest, u8 mode);

bool Fs_StageCdfIsAvailable(u32 stageIdx);

/// Results of uploading a complete compressed image rectangle.
enum {
    FILE_SYSTEM_IMAGE_UPLOAD_COMPLETE     = 0,
    FILE_SYSTEM_IMAGE_UPLOAD_RETRY        = 0x7F,
    FILE_SYSTEM_IMAGE_UPLOAD_TIMER_FAILED = 0xFF,
};

/// Decodes a filesystem image chunk and uploads its rectangle to VRAM.
///
/// `imageChunk` is a readable, halfword-aligned 16-byte header followed by one
/// complete LZSS payload with a zero-offset end token and one readable byte of
/// lookahead. Both the decoded byte count and the upload's `w * h` halfwords,
/// rounded up to a whole word, must fit the shared 4480-byte staging buffer.
/// Neither extent is checked. Width must be in 1..1024 and height in 1..512.
/// The GPU reads the whole rectangle even when the decoder produces fewer
/// bytes, retaining the previous buffer contents in that tail. Width counts
/// VRAM halfwords; height and Y count rows. Source rows 245..255 receive the signed
/// Y shift; relocation mode adds one row for every source Y. The resulting
/// rectangle must fit VRAM. Input is borrowed only for this call.
///
/// Requires counter 2 to be running and an initialized scratch stack whose
/// cursor is above byte 256. The decoder uses the uncleared scratchpad history
/// ring at bytes [0, 256); back-references must use bytes written by this payload
/// or valid prior contents. Shares filesystem decode/GPU state and cannot be
/// reentered; the current image-load slot must be in 0..11.
///
/// Zero `ignoreGpuTimeLimit` permits an abort at 28224 counter-2 ticks while
/// suspending drawing or after upload. Nonzero waits without that cutoff; GPU
/// completion always waits without a timeout. Drawing resumes before return.
/// Returns FILE_SYSTEM_IMAGE_UPLOAD_COMPLETE on success,
/// FILE_SYSTEM_IMAGE_UPLOAD_RETRY on scratch contention or elapsed cutoff,
/// and FILE_SYSTEM_IMAGE_UPLOAD_TIMER_FAILED if the counter cannot reset.
/// The SDK upload result is ignored; COMPLETE does not report SDK failures.
/// RETRY can follow an already completed upload. Repeating a chunk also repeats
/// its decode and preserves any tail bytes the stream does not overwrite.
u8 fsUploadImageChunk(const FsImageChunk* imageChunk, u8 ignoreGpuTimeLimit);

/// Caption choices for a location loading screen.
enum {
    GAME_FLOW_LOAD_CAPTION_NORMAL    = 0,
    GAME_FLOW_LOAD_CAPTION_ALTERNATE = 1,
};

/// Arms the location loading screen and resets its image workspace.
///
/// Copies only the destination's stage and area; the key need not survive
/// this call. Zero selects the normal caption, nonzero its story-event variant
/// where available. The image workspace must be initialized and available for
/// clearing. Sets two-VBlank frame timing; later updates start the image load
/// and run the caption/fade sequence.
void gameFlowBeginLoadScreen(const GameLocationKey* destination, s16 alternateCaption);

void Fs_EnsureBootLoadStarted(void);

/// Borrows the remainder of the current chunk's opening sector.
///
/// Text chunks stay here because their loader copies no payload. Valid bytes
/// end at the chunk header's sectorLen; the rest of the returned 0x7F0-byte
/// view includes sector pad. The next sector read overwrites this storage.
u8* fsGetChunkPayload(void);

/// Empty entry point called on scripted scene view changes; its intended role is unproven.
void cdCmdSceneViewChangeNoOp(void);

#endif // MAIN_FS_H
