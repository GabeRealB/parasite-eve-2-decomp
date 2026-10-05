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
/// replacement, and retains no source pointers. `CdCmd_CommitReplace` later
/// appends it to the ring; a zero command marks the replacement empty.
void cdCmdStageReplacement(s32 command, const void* fileKey, const void* commandArgs);

s32 CdCmd_CommitReplace(void);

s32 CdCmd_DropPending(void);

u16 CdCmd_IsIdle(void);

u16 CdCmd_IsSlotEmpty(s16 slot);

void CdCmd_SetBusy(void);

void CdCmd_ResetEntryIter(void);

CdCmdEntry* CdCmd_NextEntry(void);

/// Saves all eight bytes of the ring head in the active-request snapshot.
///
/// Does not advance the ring or alter the saved resume sector or dispatch
/// phase. The saved bytes survive retirement or reuse of the ring slot.
void cdCmdSaveHeadRequest(void);

void CdCmd_AdvanceRead(void);

s32 CdCmd_ActivatePhase1(void);

s32 CdCmd_PollStatus(s32 arg0, s32 arg1);

void CdCmd_EnqueueOverlay81(void);

void CdCmd_EnqueueOverlay82(void);

void CdCmd_EnqueueReplaceOverlay82(void);

/// Empty entry point in the caption/scene-control handshake; its intended role is unproven.
void cdCmdSceneControlNoOp(void);

void CdCmd_CancelReplaceAndActivate(void);

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

void CdCmd_SelectMdecBuffer(void);

void CdCmd_StartOverlay(u16 arg0, u16 arg1, u16 arg2);

void CdCmd_EnqueueLoadFile(s32 arg0, s32 arg1, s32 arg2);

void CdCmd_StepVlcRebuild(void);

/// Enqueues `entry`'s command again, rebuilding from its fields the two
/// parameter blocks `cdCmdEnqueue` unpacks into a slot.
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
    return cdCmdEnqueue(entry->cmd, paramA, paramB);
}

void Fs_ReadSectorEx(s32 sector, s32 endSector, u8* dest, u8 mode);

bool Fs_StageCdfIsAvailable(u32 stageIdx);

u8 Fs_LoadImageChunk(FsImageChunk* img, u8 retryNonzero);

void Fs_BeginBootLoad(u8* arg0, s16 arg1);

void Fs_EnsureBootLoadStarted(void);

u8* Fs_GetChunkPayload(void);

void CdVol_SetMixMode(s32 stereo);

/// Empty entry point called on scripted scene view changes; its intended role is unproven.
void cdCmdSceneViewChangeNoOp(void);

#endif // MAIN_FS_H
