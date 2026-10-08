#ifndef MAIN_PRIVATE_FS_H
#define MAIN_PRIVATE_FS_H

#include <psyq/sys/types.h>
#include <psyq/libcd.h>

#include "types.h"

#include "fs_types.h"
#include "main/text.h"

extern s32 Fs_ReqSector;

extern s32 Fs_VBlank;

extern u8 Fs_ChunkMode;

extern u8 Fs_CdErrorCount;

extern s32 Fs_StageCdfSectors[FS_CDF_STAGE_COUNT];

extern FsSector Fs_CdSector;

extern s16 Fs_BootLoadSlot;

extern u16 Fs_BootLoadPhase;

extern u16 D5B498_8006AC9C;

extern void* Fs_BootTimSecondary;

extern void* Fs_BootTimPrimary;

extern BootLoadDestination Fs_LoadParams;

extern s16 D5B498_8006ACC0;

extern s8 D5B498_8006C233;

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

/// Status halfwords reported through `D5B498_8006D748` by `fsDecompressStream`.
enum {
    FILE_SYSTEM_STREAM_DECODE_NEEDS_INPUT  = 0,
    FILE_SYSTEM_STREAM_DECODE_COMPLETE     = 1,
    FILE_SYSTEM_STREAM_DECODE_SCRATCH_BUSY = 0xFFFF,
};

/// Decodes an LZSS stream into RAM until its input boundary or end token.
///
/// Set `Fs_ChunkReadPtr` to readable compressed bytes and `Fs_ChunkWritePtr` to
/// sufficient output storage. For sector-fed decoding, `D_8006C4D4` is the
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
/// Before a new stream, clear `D5B498_8006EBB0` (unread-bit mask),
/// `D5B498_8006EA1A` (input byte), `D5B498_8006D850` (resume address) and
/// `D5B498_8006D748` (status), and set `D5B498_8006D858` (ring write index) to 1.
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

/// Status halfwords produced by `fsDecompressImagePayload`.
enum {
    FILE_SYSTEM_IMAGE_DECODE_COMPLETE     = 1,
    FILE_SYSTEM_IMAGE_DECODE_SCRATCH_BUSY = 0xFFFF,
};

/// Decodes one complete image/CLUT LZSS payload into caller-provided RAM.
///
/// Set `Fs_ChunkReadPtr` to the compressed bytes after the image header and
/// `Fs_ChunkWritePtr` to storage for the entire decoded payload. Input must
/// contain a zero-offset end token and allow one byte of lookahead; neither
/// input nor output bounds are checked. Both global pointers are left intact.
///
/// Uses scratchpad bytes [0, 256) as a ring, starting its write index at 1.
/// The ring is not cleared: references to unwritten slots require valid prior
/// contents. The initialized scratch-stack cursor must have a byte offset
/// greater than 256; otherwise no payload is read or written. Reports
/// FILE_SYSTEM_IMAGE_DECODE_COMPLETE or FILE_SYSTEM_IMAGE_DECODE_SCRATCH_BUSY
/// through `D5B498_8006D748`. Decoding is synchronous and does not upload to VRAM.
void fsDecompressImagePayload(void);

/// Builds folder sector offsets from the mounted stage's first CDF sector.
///
/// The caller must already have read that sector into `Fs_CdSector`.
/// `unusedStageIndex` is ignored. Input ends at a zero sectorCount and must fit
/// the 50-record runtime table; later folder requests require matching IDs.
void fsInitFolderTable(s32 unusedStageIndex);

/// Reads the first sector of a stage CDF into the shared folder-list buffer.
///
/// Uses the low byte of `stageIndex`, which must be in 1..5 and name a CDF on
/// the current disc. Requires serialized access in sector-header mode. Resets
/// image load mode/slot state; starts one raw sector asynchronously. Poll
/// `Fs_CdOpStatus` (0xFF done, 0x80 restart), then call `fsInitFolderTable`.
void fsStartStageFolderListRead(s32 stageIndex);

/// Starts rebuilding the STAGE0 file/stream tables from its HED asynchronously.
///
/// Requires sector-header mode, the ISO scan's HED and STAGE0.CDF sectors,
/// and no competing transfer.
/// Resets append counts without clearing table storage, then reads until the
/// HED terminator. Poll `Fs_CdOpStatus` for completion (0xFF) or restart (0x80).
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
/// those stage rules. Requires a stage index within `Fs_StageCdfSectors`.
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
/// releasing storage. Starts one raw sector into `Fs_CdSector`; requires serialized
/// sector-header reads. Poll `Fs_CdOpStatus` (0xFF done, 0x80 restart), then build
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
/// stores that slot in `Fs_BootLoadSlot` for asynchronous completion polling.
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
/// Install after `sndLoadBeginSectorLoad`, with `Fs_ReqSector` set to the first
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
