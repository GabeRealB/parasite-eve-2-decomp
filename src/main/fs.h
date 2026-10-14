#ifndef MAIN_PRIVATE_FS_H
#define MAIN_PRIVATE_FS_H

#include <psyq/sys/types.h>
#include <psyq/libcd.h>

#include "types.h"

#include "fs_types.h"
#include "main/session_types.h"
#include "main/text.h"

/// Absolute CD sector expected by the next filesystem or sound-bank ready callback.
///
/// Read setup selects the start; accepting a sector advances it before payload
/// processing. A retry can decrement it to repeat the sector. Recovery resumes
/// at this cursor; it is not the last sector received or a CDF-relative offset.
extern s32 gFsNextReadSector;

/// VBlank counter sample for the current CD operation's timeout interval.
///
/// Starting/reissuing a read or accepting progress refreshes it with `VSync(-1)`.
/// The timeout handler restarts pending/resuming operations after 180 VBlanks;
/// it retains the original signed counter arithmetic and comparison.
extern s32 gFsCdProgressVBlank;

/// Modulo-256 count of errors since the last accepted read-start or seek completion.
///
/// Shared CDF and sound-bank callbacks increment it for drive/position errors;
/// read-start/seek success clears it. It neither sets a retry limit nor saturates.
extern u8 gFsCdErrorCount;

/// Absolute starting sectors of STAGE0.CDF through STAGE5.CDF on the scanned disc.
///
/// Zero marks an absent CDF. ISO discovery clears all six entries and publishes
/// discovered LBAs; file/folder/stream offsets add the selected stage's base.
/// Requests require stage indices 0..5 and a present file. Cached until a new
/// disc-directory scan, with no ownership of the disc contents.
extern s32 gFsStageCdfBaseSectors[FILE_SYSTEM_CDF_STAGE_COUNT];

/// Resident two-sector scratch storage shared by filesystem and CD audio reads.
///
/// `sector` is overwritten by the next header/directory/chunk/payload read;
/// `clutContinuation` retains its bytes until a two-sector CLUT replaces them.
/// CLUT decoding spans both sectors. Audio borrows the whole allocation for
/// its 16-byte-offset, 2048-byte SPU transfer, including the retained trailing
/// 16 bytes. Finish CD/decode/SPU consumers before another operation reuses it.
extern FsSectorBuffers gFsSectorBuffers;

/// Idle/armed phase shared by loading-screen reset and begin operations.
enum { GAME_FLOW_LOAD_SCREEN_IDLE = 0 };

/// CD request-ring slot whose retirement releases the selected loading image.
///
/// Set by image enqueue; valid indices are 0..7. Poll only in the read-pending
/// phase, before the retired ring slot can be reused for another request.
extern s16 gGameFlowLoadImageSlot;

/// Outer loading-screen phase (0 idle/armed, 1 image read pending, 2 presenting).
///
/// Beginning a screen arms phase zero; image enqueue and retirement advance it.
/// Finishing the presentation returns it to zero and releases the active latch.
extern u16 gGameFlowLoadScreenPhase;

/// Loading presentation step (0 begin, 1 reveal image, 2 caption, 3 hold, 4 fade out).
///
/// Reset when the image request retires; advanced once each phase completes.
extern s16 gGameFlowLoadScreenPresentationStep;

/// Retained optional secondary loading caption; NULL when no pair is selected.
///
/// Borrows resident writable text records. The presentation interface ignores
/// this selection and retains its delayed completion path without drawing it.
extern TextStream* gGameFlowLegacySecondaryLoadCaption;

/// Selected resident loading caption, whose cursor is reset and advanced for playback.
///
/// Borrows its record, glyph script and font cells through hold and fade-out.
/// Selection finishes before image enqueue; presentation starts after retirement.
extern TextStream* gGameFlowLoadCaption;

/// Location a boot load is entering; its stage and area select the
/// loading-screen image and caption.
///
/// Beginning a screen copies only `stage` and `area` from its destination
/// key; its other members are never read or written. That it is a whole
/// `GameLocationKey` is inferred from those two sitting at the key's offsets,
/// not established by an access.
extern GameLocationKey gGameFlowLoadScreenDestination;

/// Story-caption request (0 normal, any nonzero value alternate where available).
///
/// Retained as the caller's signed halfword for the gas-station, Shelter R36
/// and default Neo Ark/Shelter heliport selectors; other areas ignore it.
extern s16 gGameFlowLoadAlternateCaption;

/// Signed X displacement of relocated image columns, in 64-halfword VRAM pages.
///
/// Applied only when a column's recorded Y is at least 256 or relocation mode
/// is selected. File loading narrows its request to s8; scene-image uploads
/// temporarily replace it. Complete-image headers do not use this displacement.
extern s8 gFsImageXPageOffset;

/// Zeroes the entire resident CD state, including requests and buffer pointers.
///
/// Used after drive initialization/reset. Transfers and buffer use must have
/// ended first; this neither stops the drive nor frees playback allocations.
void cdCmdResetState(void);

/// Releases the queue's blocking-operation flag and display CD-busy latch.
///
/// The display latch is written only when the queue was busy. Ring indices and
/// dispatch phase are unchanged, so this does not imply `cdCmdIsIdle`.
void cdCmdClearBusy(void);

/// Appends a request only when the ring head is outside the scene-audio family.
///
/// Tests the head opcode's high nibble (0x8_), rather than scene playback mode
/// or movie commands. Uses `cdCmdEnqueue`'s byte-source and ring-capacity
/// contract. A blocked request performs no source reads and reports no result.
void cdCmdEnqueueUnlessSceneAudioPending(s32 command, const void* fileKey, const void* commandArgs);

/// Services the current CD request phase, then advances an active loading screen.
///
/// Normal dispatch selects the ring head's opcode family unless suspended;
/// cancellation and suspension instead service the saved active request.
/// The ring read index must be 0..7 and request resources must remain valid
/// until their handler retires them. Empty or unsupported families do nothing.
/// Call from the main-loop/task CD service, serialized with other dispatches;
/// one call may start asynchronous work or wait for drive recovery.
/// Loading-screen presentation advances even when normal dispatch is suspended.
void cdCmdDispatch(void);

/// Services CD requests and loading presentation from either frame loop.
///
/// Uses `cdCmdDispatch`'s serialized request/resource contract. Call once per
/// main-loop or task-presentation iteration, including paused-game iterations;
/// a service call may block for drive recovery rather than retire a request.
void cdCmdService(void);

/// Requests suspension of a non-scene-audio ring head for a display transition.
///
/// Returns 1 if cancellation/suspension is already selected. Otherwise an empty
/// head or scene-audio-family head returns 0; any other opcode is snapshotted
/// and selects suspension, returning 1. Dispatch performs the actual stop and
/// captures a movie's absolute resume sector; this call does not wait for it.
u16 cdCmdRequestSuspend(void);

/// Requeues a saved movie and polls until it starts or the queue becomes idle.
///
/// Call repeatedly after suspension and intervening loads. Movie-family requests
/// resume through `CD_COMMAND_CONTINUE_STREAM`; offset-movie requests use
/// `CD_COMMAND_RESUME_STREAM_AT_POSITION` and the saved absolute sector. Returns
/// 0 while waiting, then clears the complete active snapshot and returns 1.
/// Other saved opcode families return 1 without changing the snapshot. Enqueue
/// requires free ring capacity as in `cdCmdEnqueue`.
u16 cdCmdResumeSuspendedMovie(void);

/// Issues a logical seek and polls until it completes, retrying drive errors.
///
/// Returns 0 while pending and 1 on completion, resetting the seek step.
/// Start with `seekStep` at CD_COMMAND_SEEK_SET_LOCATION. `location` supplies
/// three BCD minute/second/sector bytes (00..99, 00..59, 00..74); its track
/// byte is ignored. Keep the same target across polls; the pointer is read
/// synchronously only when issuing Setloc and is never retained. The second
/// argument is ignored. Requires serialized use of the resident drive state.
u16 cdSyncPollLogicalSeek(CdlLOC* location, s32 unused);

/// Returns 1 when a blocking NOP response contains latched shell-open status.
///
/// Returns 0 otherwise. This reports the drive's "shell was opened" bit,
/// rather than guaranteeing that the shell is currently open. Shares the
/// command channel with other CD operations; requires serialized drive use.
s16 cdSyncHasShellOpenStatus(void);

/// Issues pause and polls completion, flushing and reissuing on retry.
///
/// Returns 0 while pending and 1 when pause completes, resetting `pauseStep`.
/// Start with `pauseStep` at CD_COMMAND_PAUSE_ISSUE. Shares command recovery
/// with seek and movie playback; requires serialized use of the resident state.
u16 cdSyncPollPause(void);

/// Advances disc recovery through readiness, mode setup and a read probe.
///
/// Returns 1 unless the ready drive's probe reports both CdlStatError and
/// response-byte-1 bit 0x40, resetting `diskRecoveryStep`; other polls return 0.
/// Mode setup selects double speed and sector headers and waits three VBlanks.
/// The SDK positions the probe using raw minute/second/sector bytes {0x0A, 0, 0};
/// the minute is not valid packed BCD and the intended target is unproven.
/// A rejected probe enters a state that polls shell-open status and resets
/// movie playback to its initial step, but never returns completion or leaves
/// that recovery state. Requires serialized drive use; command calls block.
s16 cdSyncPollDiscRecovery(void);

/// Shared LZSS status halfwords and initial history cursor.
enum {
    FILE_SYSTEM_LZSS_DECODE_NEEDS_INPUT  = 0,
    FILE_SYSTEM_LZSS_DECODE_COMPLETE     = 1,
    FILE_SYSTEM_LZSS_DECODE_SCRATCH_BUSY = 0xFFFF,
    FILE_SYSTEM_LZSS_INITIAL_WRITE_INDEX = 1,
};

/// Shared decoder result (0 unfinished, 1 end token, 0xFFFF scratch stack busy).
///
/// Callers seed zero before a stream. Suspension leaves it unchanged; the
/// resumable and complete-image decoders both write completion or refusal.
/// Scratch refusal must be cleared before resuming an unfinished stream.
extern u16 gFsLzssDecodeStatus;

/// Read-only compressed-byte cursor shared by both LZSS decoders.
///
/// The resumable decoder advances/saves it on suspension and completion; the
/// complete-image decoder leaves the supplied base intact. Sources remain
/// readable through their decode, including eager lookahead. The pointer itself
/// is mutable; no decoder writes through it. Strip scanning also advances it.
extern const u8* gFsLzssInputCursor;

/// Next writable byte of a raw CDF payload or LZSS decoder destination.
///
/// Chunk setup selects borrowed RAM storage. Raw/bundle copies advance by bytes;
/// stream decoding advances/saves it, while complete-image decoding retains
/// its supplied base. The caller must establish writable capacity and preserve
/// the storage through CD/decode/GPU completion; this cursor owns no allocation.
extern u8* gFsPayloadWriteCursor;

/// Saved interior program counter for an unfinished resumable LZSS token.
///
/// NULL starts at the next token flag. Otherwise the decoder restores its live
/// state, clears this pointer, and jumps directly to it; it is not a C-callable
/// callback. Keep the decoder code and saved state intact until completion.
extern void* gFsLzssResumeAddress;

/// Next write index (0..255) in the resumable decoder's scratchpad history ring.
///
/// Seed 1 for a new stream. Output advances it modulo 256; suspend/complete
/// saves it. Stored at halfword width by the handwritten decoder.
extern u16 gFsLzssRingWriteIndex;

/// Number of unread bits (0..8) in the resumable decoder's loaded input byte.
///
/// Refills set 8 and token extraction subtracts consumed bits. A strip reset
/// clears it; other new-stream paths clear the mask and refill before using it.
/// Suspension preserves it, using halfword loads and stores.
extern u16 gFsLzssUnreadBitCount;

/// Last byte loaded by the resumable decoder, retained across input intervals.
///
/// Its value is 0..255, but assembly saves/restores a halfword. The cursor can
/// already point at the next sector while this reservoir still supplies bits.
/// New-stream setup clears it without clearing the scratchpad history.
extern u16 gFsLzssInputByte;

/// Low-bit mask of unread reservoir bits (0 or 2^n-1 for n=1..8).
///
/// Zero forces a byte refill; setup clears it, and suspension preserves it with
/// the input byte/count. Stored at halfword width by the handwritten decoder.
extern u16 gFsLzssUnreadBitMask;

/// Exclusive byte boundary at which the sector-fed LZSS decoder suspends input.
///
/// Points into the current CD sector at its header's valid-byte end, reused
/// for continuation sectors; startup seeds the whole-sector end. The decoder
/// tests equality after advancing its cursor, preserving the loaded byte for
/// resumption. A complete RAM stream can instead end by token without reaching
/// this boundary, so this pointer does not establish that stream's RAM extent.
extern const u8* gFsLzssInputEnd;

/// Decodes an LZSS stream into RAM until its input boundary or end token.
///
/// Set `gFsLzssInputCursor` to readable compressed bytes and `gFsPayloadWriteCursor` to
/// sufficient output storage. For sector-fed decoding, `gFsLzssInputEnd` is the
/// exclusive end of the current nonempty input interval. Complete RAM streams
/// may instead leave that boundary outside the addresses their cursor reaches.
/// Bits are read MSB first: a 1 flag precedes an eight-bit literal; a 0 flag
/// precedes an eight-bit absolute history-ring index (0 ends the stream), then
/// a four-bit length plus 2, giving matches of 2..17 bytes. No output bound is
/// checked. Byte reads advance the input cursor before testing boundary equality;
/// reading a literal or match index eagerly fetches the following byte, even
/// when aligned. Complete RAM input needs one byte of readable lookahead; the
/// boundary must not cause suspension before the end token is processed.
///
/// Before a new stream, clear `gFsLzssUnreadBitMask` (unread-bit mask),
/// `gFsLzssInputByte` (input byte), `gFsLzssResumeAddress` (resume address) and
/// `gFsLzssDecodeStatus` (status), and set `gFsLzssRingWriteIndex` (ring write index) to 1.
/// Other saved token fields are initialized as decoding reaches them.
/// Both byte cursors are saved on suspension or completion. On NEEDS_INPUT the
/// last loaded byte and partial token are saved: supply the next nonempty input
/// interval without resetting decoder state or moving the output cursor.
/// Suspension leaves status unchanged, so it must already be NEEDS_INPUT.
/// COMPLETE marks the zero-index end token. SCRATCH_BUSY changes only status;
/// clear it before retrying an unfinished stream.
///
/// Uses scratchpad bytes [0, 256) as an uncleared history ring. The initialized
/// scratch-stack cursor's byte offset must be greater than 256 or decoding is
/// refused. Keep the ring and saved globals intact until completion; decoding
/// is synchronous, shares state with image-payload decoding, and is not reentrant.
/// References to unwritten ring slots require valid prior contents.
void fsDecompressStream(void);

/// Decodes one complete image/CLUT LZSS payload into caller-provided RAM.
///
/// Set `gFsLzssInputCursor` to the compressed bytes after the image header and
/// `gFsPayloadWriteCursor` to storage for the entire decoded payload. Input must
/// contain a zero-offset end token and allow one byte of lookahead; neither
/// input nor output bounds are checked. Both global pointers are left intact.
///
/// Uses scratchpad bytes [0, 256) as a ring, starting its write index at 1.
/// The ring is not cleared: references to unwritten slots require valid prior
/// contents. The initialized scratch-stack cursor must have a byte offset
/// greater than 256; otherwise no payload is read or written. Reports
/// FILE_SYSTEM_LZSS_DECODE_COMPLETE or FILE_SYSTEM_LZSS_DECODE_SCRATCH_BUSY
/// through `gFsLzssDecodeStatus`. Decoding is synchronous and does not upload to VRAM.
void fsDecompressImagePayload(void);

/// Builds folder sector offsets from the mounted stage's first CDF sector.
///
/// The caller must already have read that sector into `gFsSectorBuffers`.
/// `unusedStageIndex` is ignored. Input ends at a zero sectorCount and must fit
/// the 50-record runtime table; later folder requests require matching IDs.
void fsInitFolderTable(s32 unusedStageIndex);

/// Reads the first sector of a stage CDF into the shared folder-list buffer.
///
/// Uses the low byte of `stageIndex`, which must be in 1..5 and name a CDF on
/// the current disc. Requires serialized access in sector-header mode. Resets
/// chunk policy/load-bank state; starts one raw sector asynchronously. Poll
/// `gFsCdOperationStatus` (0xFF done, 0x80 restart), then call `fsInitFolderTable`.
void fsStartStageFolderListRead(s32 stageIndex);

/// Starts rebuilding the STAGE0 file/stream tables from its HED asynchronously.
///
/// Requires sector-header mode, the ISO scan's HED and STAGE0.CDF sectors,
/// and no competing transfer.
/// Resets append counts without clearing table storage, then reads until the
/// HED terminator. Poll `gFsCdOperationStatus` for completion (0xFF) or restart (0x80).
/// Records must fit wholly within a sector and their destination table; indexed
/// file IDs and compact sector offsets must fit their table/halfword domains.
void fsStartStage0HeaderRead(void);

/// Blocks until a CD command completes, recovering a disc error if necessary.
///
/// Recovery waits for a closed shell and a readable CD-ROM, then restores
/// double-speed reads with sector headers. It has no timeout and does not
/// clear the filesystem's error counter or reissue the failed read.
void cdSyncWaitForCommandCompletion(void);

/// Results of the blocking tray-swap probe.
enum {
    CD_SYNC_DISC_SWAP_COMPLETE = 0,
    CD_SYNC_DISC_SWAP_ERROR    = 0xFF,
};

/// Waits for a tray open/close cycle and probes the new disc's volume sector.
///
/// Blocks without a timeout until standby and TOC readiness, then starts a
/// 2048-byte read at sector 16. A disk error returns CD_SYNC_DISC_SWAP_ERROR;
/// success pauses and restores double speed with sector headers. Does not
/// check which game disc was inserted or copy the volume descriptor to RAM.
u8 cdSyncWaitForDiscSwap(void);

/// Waits for a readable CD-ROM and restores the filesystem's read mode.
///
/// Completes the pending command, resets drive mode, and waits without a
/// timeout for a closed shell and ready CD-ROM; no tray cycle is required.
/// Nonzero `includeSectorHeader` enables headers in double-speed reads.
/// The caller manages callbacks and reissues its interrupted request.
void cdSyncWaitForReadableDisc(s8 includeSectorHeader);

/// Resets drive mode, waits three VBlanks, and stops the spindle.
void cdSyncStopDisc(void);

/// Returns the disc needed by the current stage when its CDF is absent.
///
/// Returns GAME_MAIN_DISC_1 for stages 1/2, GAME_MAIN_DISC_2 for stages 4/5,
/// and GAME_MAIN_DISC_UNKNOWN when available or when no disc is selected by
/// those stage rules. Requires a stage index within `gFsStageCdfBaseSectors`.
s32 fsGetRequiredStageDisc(void);

/// Finds the game files and disc number in the disc's ISO directory sector.
///
/// Requires serialized sector-header reads. Checks logical sector 22, falling
/// back to 20 when its opening directory record is absent. Accepts the retail
/// directory's even record lengths 48..62; names must be NUL-terminated inside
/// the sector, CDF suffix digits must be in 0..5, and all inspected records and
/// halfwords must fit. This is the game's fixed layout, not a general ISO walk.
/// The low byte of `bootMode` selects startup behavior: nonzero also reads the
/// first 16 sectors of INIT.BS into the image workspace and restarts if the HED
/// is absent. Zero omits that image and reports an unknown disc without a HED.
/// Blocks through directory/image reads and drive recovery without a timeout,
/// then starts the HED table read asynchronously when present. Calls SetMem(2).
void fsScanIsoDirectory(s32 bootMode);

/// Input policies for a prepared strip-image transfer (only the low byte is used).
enum {
    FILE_SYSTEM_IMAGE_STRIPS_CD_INPUT       = 0,
    FILE_SYSTEM_IMAGE_STRIPS_RESIDENT_INPUT = 1,
};

/// Results of advancing a strip-image transfer; RETRY can follow a VRAM upload.
enum {
    FILE_SYSTEM_IMAGE_STRIPS_NEEDS_INPUT  = 0,
    FILE_SYSTEM_IMAGE_STRIPS_COMPLETE     = 1,
    FILE_SYSTEM_IMAGE_STRIPS_RETRY        = 0x7F,
    FILE_SYSTEM_IMAGE_STRIPS_TIMER_FAILED = 0xFF,
};

/// Decodes and uploads sequential 64-halfword by 32-row strips to VRAM.
///
/// Requires `fsBeginImageColumns` setup, exclusive filesystem decode/GPU state,
/// counter 2 running, and initialized scratch history as for `fsDecompressStream`.
/// Every strip's decoded bytes must fit the shared 4480-byte staging buffer;
/// the GPU reads 4096 bytes even when decoding leaves its old tail intact.
/// The prepared rectangles must fit VRAM and the table storage must survive
/// the transfer. Later columns receive the X-page shift but use their stored Y.
///
/// A zero low byte of `inputMode` means CD input: NEEDS_INPUT/COMPLETE resets the
/// cursor for the next sector and permits aborts at 28224 counter-2 ticks.
/// Nonzero means resident input: preserves the cursor and ignores that cutoff.
/// The zero-byte separator scan stops after six bytes or at the CD buffer end,
/// even for resident input, returning NEEDS_INPUT.
/// COMPLETE means the column terminator was reached. RETRY reports scratch
/// contention or a cutoff, including after an upload; restart with
/// `fsBeginImageColumns` for resident input or filesystem read recovery for CD
/// input. TIMER_FAILED leaves drawing untouched. Drawing resumes on all other
/// returns; GPU-idle waits themselves have no timeout.
u8 fsUploadImageStrips(s32 inputMode);

/// Copies a column table and primes the first strip of its compressed image.
///
/// The table must have at least two readable records, with an X terminator
/// among its first 31 records. Its first dataOffset locates the compressed
/// stream in bytes from `table`; that storage must survive the transfer.
/// Columns are 64 VRAM halfwords wide and strips are 32 rows high. High-VRAM
/// columns and mode 2 apply the configured X-page shift; mode 2 also adds 128
/// rows to the first column only. A second-record terminator can override the
/// default 256-row column height. No storage is allocated or GPU upload started.
void fsBeginImageColumns(const FsImageColumn* table);

/// Resolves a file key and starts its asynchronous CDF read or seek.
///
/// Borrows the key only for this call. Stage zero uses the mounted library's
/// category tables; stages 1..5 require the correct folder table already loaded.
/// Direct file indices and counted table lengths must fit their tables; the
/// stage-folder file table's full extent is currently unproven.
/// Uses the low byte of `loadMode` (`CD_COMMAND_LOAD_*`, unknown values mean
/// normal). Image offsets narrow to signed bytes: X counts 64-halfword VRAM
/// pages for high-VRAM/relocated strips; Y counts rows for headers at y=245..255.
/// Stage-zero category 4 accepts only hundreds=1, starts a sound-bank read and
/// ignores these options. Serialize with other filesystem reads. Returns the
/// absolute CD sector's low 16 bits, or zero when resolution fails; completion
/// and read errors are reported asynchronously through filesystem state.
s32 fsLoadFile(const FsFileLoadKey* fileKey, s32 loadMode, s32 imageXPageOffset, s32 imageYOffset);

/// Invalidates published folder slots and reads the requested folder directory.
///
/// Uses the arguments' low bytes: `stageIndex` must name a mounted stage CDF
/// (1..5) and `fileGroup * 100 + folderIndex` must exist in its folder table.
/// Clears resource kinds, stream start sectors and bundle redirect state without
/// releasing storage. Starts one raw sector into `gFsSectorBuffers`; requires serialized
/// sector-header reads. Poll `gFsCdOperationStatus` (0xFF done, 0x80 restart), then build
/// the file/stream tables with `fsBuildFolderTables`.
void fsStartFolderDirectoryRead(s32 stageIndex, s32 fileGroup, s32 folderIndex);

/// Builds file and stream lookup tables from a loaded folder directory sector.
///
/// Uses the low bytes of the arguments: files are rebased from folder
/// `fileGroup * 100 + folderIndex`, streams from folder `fileGroup * 100 + 1`.
/// File sectors remain relative to the stage CDF; stream sectors include its
/// disc base. Rebases the buffered stream descriptors in place before copying
/// each complete record. Both folder IDs must exist in the mounted table.
/// The file list must terminate at zero sectorOffset within its sector view,
/// with file IDs inside the destination table's extent (currently unproven).
/// The stream list must end at a zero key with at most 15 active records.
void fsBuildFolderTables(s32 stage, s32 fileGroup, s32 folderIndex);

/// Polls the selected loading image, then advances its caption and fade presentation.
///
/// Call once per presentation frame while bootLoadActive is set. The queued
/// image request owns the load until its ring slot empties; only then are the
/// presentation cursor and caption reveal started. Selected captions and the
/// decoded image must remain available through fade-out.
void gameFlowStepLoadScreen(void);

/// Reissues ReadN at the filesystem's retained absolute request sector.
///
/// Serialize with the current file/mount request and its retained destination.
/// A pending drive error blocks until a closed, readable CD-ROM is available,
/// restoring double-speed sector-header mode. For chunk-resume state 0x40,
/// reinstalls the sync callback, timestamps the read and advances to 0x41;
/// other operation states are retained. Completion remains asynchronous.
void fsResumeRequestedRead(void);

/// Aborts a pending seek/read after more than 180 VBlanks without progress.
///
/// Applies to the ordinary pending state (0) and a chunk read being resumed
/// (0x41). Uses the absolute VSync counter, not rendered frames. Selects request
/// restart (0x80), flushes the drive, releases sound-load state, removes both CD
/// callbacks and waits for pause completion. Does not increment the error count.
void fsAbortTimedOutOperation(void);

/// Clears the loading display, selects resident captions and queues its image read.
///
/// Uses the destination and caption selector retained by `gameFlowBeginLoadScreen`.
/// Clears the full resident image workspace and both 320x240 framebuffers, then
/// blanks the display until the image is ready. Requires a free CD request slot;
/// stores that slot in `gGameFlowLoadImageSlot` for asynchronous completion polling.
/// All image files come from category zero of the global stage-zero library.
void gameFlowStartLoadScreenImage(void);

/// Steps loading-screen reveal, caption playback, the minimum hold and fade-out.
///
/// Call once per presentation frame after the image load has finished. The
/// mutable primary caption and its text/font storage must remain live until
/// completion. The fade mask changes by 16 RGB levels per frame; the retained
/// secondary delay is 61 frames, and the post-caption hold is at least 60
/// frames, extended by holdBootImage.
/// legacySecondaryCaption is retained in the interface but ignored; secondary
/// drawing is suppressed. Completion releases bootLoadActive and the load phase.
void gameFlowStepLoadScreenPresentation(TextStream* primaryCaption, TextStream* legacySecondaryCaption);

/// Validates and feeds whole CD sectors to the active sound-bank sequence load.
///
/// Install after `sndLoadBeginSectorLoad`, with `gFsNextReadSector` set to the first
/// absolute sector. The borrowed payload buffer must be word-aligned, writable
/// for 2048 bytes and remain live until its SPU DMA finishes. Selects polling
/// uploads. Every interrupt except CdlDiskError reads a 12-byte header then the
/// payload. Position mismatch, drive error or feeder failure pauses delivery,
/// increments the wrapping byte error count and requests restart (0x80).
/// DONE pauses delivery, installs the sequence and reports completion (0xFF),
/// even if installation fails. Removes the ready callback on either outcome.
/// SDK result bytes are unused; CD command and buffer use must be serialized.
void fsSoundBankReadyCallback(u8 interruptStatus, u8* unusedResult);

/// Starts the fade cursor at the retained left CD gain's seven-bit level.
///
/// Reads software attributes last applied by this module; does not query or
/// change hardware. Call before `cdVolStepFadeOut` when fading an active movie.
void cdVolBeginFadeOut(void);

/// Applies a movie's CD volume preset equally to both SPU CD inputs.
///
/// `presetIndex` selects 0..39; all other u16 values select silent preset zero.
/// Stores the selected seven-bit level in the fade cursor as well. One level
/// unit is 256 SPU register units; other common output settings are retained.
void cdVolApplyMoviePreset(u16 presetIndex);

/// Lowers the fade cursor by eight levels, clamps at zero and applies both gains.
///
/// Returns the remaining level (0..127), with zero indicating silence. The
/// cursor must have been initialized by `cdVolBeginFadeOut` or a movie preset.
/// Each call advances one step; the caller controls the cadence.
s32 cdVolStepFadeOut(void);

/// Returns 1 for an idle queue or a scene-audio-family opcode at its head.
///
/// The scene-audio test accepts that head even during cancellation/suspension,
/// allowing display-transition loads to proceed alongside the scene session.
/// Every other nonidle queue returns 0.
u16 cdCmdIsIdleOrSceneAudioPending(void);

#endif // MAIN_PRIVATE_FS_H
