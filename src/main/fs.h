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

void CdCmd_ClearBusy(void);

/// Appends a request only when the ring head is outside the scene-audio family.
///
/// Tests the head opcode's high nibble (0x8_), rather than scene playback mode
/// or movie commands. Uses `cdCmdEnqueue`'s byte-source and ring-capacity
/// contract. A blocked request performs no source reads and reports no result.
void cdCmdEnqueueUnlessSceneAudioPending(s32 command, const void* fileKey, const void* commandArgs);

void CdCmd_Dispatch(void);

u16 CdCmd_ActivatePhase2(void);

u16 CdCmd_EnqueueFollowUp(void);

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

void Fs_InitFolderTable(s32 stageIdx);

void Fs_SelectStage(s32 stageIdx);

void Fs_InitStage0Tables(void);

void Fs_ClearDiskError(void);

u8 Fs_WaitDiskSwap(void);

void Fs_WaitDiskReset(s8 withSectHdr);

void Fs_StopCd(void);

s32 Fs_GetStageDiskKind(void);

void Fs_ScanIsoDirectory(s32 mode);

/// Load an image chunk into VRAM (BreakDraw / LoadImage2 path).
/// `retryNonzero` disables timeout aborts when non-zero.
u8 Fs_LoadImageStrip(s32 arg0);

/// Copy a terminated FsImageColumn list into Fs_WorkEntries and set up
/// Fs_ImageRect / load state for the following image transfer.
void Fs_CopyWorkEntries(FsImageColumn* arg0);

/// Look up a packed file id and start a CD load.
/// Returns the resolved absolute sector (low 16 bits), or 0 on failure.
s32 Fs_LoadFile(u8* req, s32 mode, s32 a2, s32 a3);

/// Look up folder `arg1*100+arg2` under stage `arg0` and start a CD read of
/// that folder into `Fs_CdSector` (cmd-queue load path).
void Fs_PrepareFolderLoad(s32 arg0, s32 arg1, s32 arg2);

/// After a folder sector is in `Fs_CdSector`: resolve file-list offsets into
/// `D_8006C158` for folder `arg1*100+arg2`, then copy stream descriptors from
/// sector+0x514 into `Stream_Slots` for folder `arg1*100+1`, adjusting offsets
/// by the folder base and `Fs_StageCdfSectors[arg0]`.
void Fs_BuildFolderTables(s32 arg0, s32 arg1, s32 arg2);

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

/// Reports an idle queue or an overlay command at its head.
extern u16 CdCmd_IsIdleOrOverlayPending(void);

#endif // MAIN_PRIVATE_FS_H
