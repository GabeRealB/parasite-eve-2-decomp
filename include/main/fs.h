#ifndef MAIN_FS_H
#define MAIN_FS_H

#include "types.h"

#include "main/fs_types.h"
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

/// Starts STR playback; `paramB[0]` is a stream-slot index (0..14).
enum { CD_COMMAND_PLAY_STREAM = 0x61 };

s32 CdCmd_Enqueue(s32 cmd, u8* paramA, u8* paramB);

void CdCmd_EnqueueReplace(s32 cmd, u8* paramA, u8* paramB);

s32 CdCmd_CommitReplace(void);

s32 CdCmd_DropPending(void);

u16 CdCmd_IsIdle(void);

u16 CdCmd_IsSlotEmpty(s16 slot);

void CdCmd_SetBusy(void);

void CdCmd_ResetEntryIter(void);

CdCmdEntry* CdCmd_NextEntry(void);

void CdCmd_LoadActiveEntry(void);

void CdCmd_AdvanceRead(void);

s32 CdCmd_ActivatePhase1(void);

s32 CdCmd_PollStatus(s32 arg0, s32 arg1);

void CdCmd_EnqueueOverlay81(void);

void CdCmd_EnqueueOverlay82(void);

void CdCmd_EnqueueReplaceOverlay82(void);

/// Unused command-module entry point; retained for the original image layout.
void CdCmd_UnusedStub0(void);

void CdCmd_CancelReplaceAndActivate(void);

void* CdCmd_SetupMdecBuffers(void);

void CdCmd_BuildVlcIfStream(void);

void CdCmd_SelectMdecBuffer(void);

void CdCmd_StartOverlay(u16 arg0, u16 arg1, u16 arg2);

void CdCmd_EnqueueLoadFile(s32 arg0, s32 arg1, s32 arg2);

void CdCmd_StepVlcRebuild(void);

/// Enqueues `entry`'s command again, rebuilding from its fields the two
/// parameter blocks `CdCmd_Enqueue` unpacks into a slot.
static inline s32 cdCmdEnqueueEntry(CdCmdEntry* entry)
{
    u8 paramA[8];
    u8 paramB[8];

    paramA[3] = entry->stage;
    paramA[2] = entry->fileGroup;
    paramA[0] = entry->fileIndex;
    paramB[0] = entry->args.bytes[0];
    paramB[1] = entry->args.bytes[1];
    paramB[2] = entry->args.bytes[2];
    paramB[3] = entry->args.bytes[3];
    return CdCmd_Enqueue(entry->cmd, paramA, paramB);
}

void Fs_ReadSectorEx(s32 sector, s32 endSector, u8* dest, u8 mode);

bool Fs_StageCdfIsAvailable(u32 stageIdx);

u8 Fs_LoadImageChunk(FsImageChunk* img, u8 retryNonzero);

void Fs_BeginBootLoad(u8* arg0, s16 arg1);

void Fs_EnsureBootLoadStarted(void);

u8* Fs_GetChunkPayload(void);

void CdVol_SetMixMode(s32 stereo);

/// Unused command-module entry point; retained for the original image layout.
void CdCmd_UnusedStub3(void);

#endif // MAIN_FS_H
