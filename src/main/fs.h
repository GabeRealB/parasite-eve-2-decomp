#ifndef MAIN_PRIVATE_FS_H
#define MAIN_PRIVATE_FS_H

#include "types.h"

#include "fs_types.h"
#include "main/task_types.h"

struct Task;

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

void CdCmd_Dispatch(void);

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

/// Polls a seek; callers supply a second argument that the routine ignores.
s32 CdCmd_SeekL(u8* loc, s32 unused);

s16 CdSync_IsShellOpenBitSet(void);

s32 CdCmd_PausePoll(void);

s16 CdCmd_RecoverDisk(void);

s32 CdCmd_StopMdec(s32 clearFb);

/// Resumable LZ/bit-stream unpack for CD chunk payloads (handwritten hasm).
/// Uses globals Fs_ChunkReadPtr / Fs_ChunkWritePtr / D5B498_8006D748; may suspend
/// mid-stream when the sector buffer ends (resume jtbl in same TU).
void Fs_DecompressChunk(void);

/// Non-resumable LZ unpack for image strips before LoadImage2 (handwritten hasm).
void Fs_DecompressImage(void);

/// Builds folder sector offsets from the mounted stage's first CDF sector.
///
/// The caller must already have read that sector into `Fs_CdSector`.
/// `unusedStageIndex` is ignored. Input ends at a zero sectorCount and must fit
/// the 50-record runtime table; later folder requests require matching IDs.
void fsInitFolderTable(s32 unusedStageIndex);

void Fs_SelectStage(s32 stageIdx);

void Fs_InitStage0Tables(void);

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

void Fs_ScanIsoDirectory(s32 mode);

/// Load an image chunk into VRAM (BreakDraw / LoadImage2 path).
/// `retryNonzero` disables timeout aborts when non-zero.
u8 Fs_LoadImageStrip(s32 arg0);

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

/// Look up a packed file id and start a CD load.
/// Returns the resolved absolute sector (low 16 bits), or 0 on failure.
s32 Fs_LoadFile(u8* req, s32 mode, s32 a2, s32 a3);

/// Look up folder `arg1*100+arg2` under stage `arg0` and start a CD read of
/// that folder into `Fs_CdSector` (cmd-queue load path).
void Fs_PrepareFolderLoad(s32 arg0, s32 arg1, s32 arg2);

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

/// Boot path: scan ISO, parse HED, load initial CDF file (file id 1).
void Boot_LoadInitialFile(struct Task* task);

void Fs_StepBootImage(void);

void Fs_RetryReadN(void);

void Fs_CheckReadTimeout(void);

/// Boot-image / CD load setup (src/main/bootload.c).
void Fs_SetupBootLoad(void);

void Fs_BootImageMachine(void* primaryTim, void* secondaryTim);

/// CD ready callback used while streaming bank data (src/main/cdvol.c).
void Fs_StreamReadyCb(u8 status, u8* result);

void CdVol_CacheFromSpu(void);

void CdVol_ApplyFromTable(u16 index);

s32 CdVol_StepDown(void);

/// Returns 1 for an idle queue or a scene-audio-family opcode at its head.
///
/// The scene-audio test accepts that head even during cancellation/suspension,
/// allowing display-transition loads to proceed alongside the scene session.
/// Every other nonidle queue returns 0.
u16 cdCmdIsIdleOrSceneAudioPending(void);

#endif // MAIN_PRIVATE_FS_H
