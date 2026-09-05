#ifndef FS_H
#define FS_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libspu.h>

// =============================================================================
// On-disk CDF / HED layout
// =============================================================================

/// Number of bytes in a CD sector.
#define FS_SECTOR_BYTE_SIZE 0x800

/// Number of words in a CD sector.
#define FS_SECTOR_WORD_SIZE 0x200

/// Resolution in bytes of the sizes/offsets in CDF/HED files.
#define FS_CDF_RESULUTION FS_SECTOR_BYTE_SIZE

/// Number of stage CDF files.
#define FS_CDF_STAGE_COUNT 6

/// Canary value at the end of the STAGE0.HED header.
#define FS_CDF_STAGE0_CANARY -1

/// Canary value at the end of the folder list.
#define FS_CDF_FOLDER_CANARY 0

/// Idle / finished value for `Fs_CdOpStatus`.
#define FS_CD_STATUS_IDLE -1

typedef enum _FsCdfStreamType {
    FS_CDF_STREAM_TYPE_MOVIE = 1,
    FS_CDF_STREAM_TYPE_AUDIO = 2,
} FsCdfStreamType;

typedef enum _FsCdfChunkType {
    FS_CDF_CHUNK_TYPE_PKG   = 0,
    FS_CDF_CHUNK_TYPE_IMG   = 1,
    FS_CDF_CHUNK_TYPE_CLUT  = 2,
    FS_CDF_CHUNK_TYPE_CAP   = 4,
    FS_CDF_CHUNK_TYPE_BG    = 5,
    FS_CDF_CHUNK_TYPE_MUSIC = 6,
    FS_CDF_CHUNK_TYPE_ASCII = 7,
} FsCdfChunkType;

typedef enum _FsCdfChunkEndFlag {
    FS_CDF_CHUNK_END_FLAG_END      = -1, // Chunk is last in file.
    FS_CDF_CHUNK_END_FLAG_CONTINUE = 1,  // File contains more chunks.
} FsCdfChunkEndFlag;

typedef struct _FsCdfStream {
    u16 type;    // Stream type.
    u16 field_2; // Unknown. The final msb is set in STAGE0.HED to differentiate streams and files.
    u32 offset;  // Movie: Offset in folder. Audio: Offset in CDF.
    union {
        struct {
            u32 interOffset;
            u16 field_c;
            u16 id;
            u16 subId;
            u16 width;
            u16 height;
            u16 field_16;
            u16 field_18;
            u16 field_1a;
            u8  field_1c[6];
            u16 field_22;
            u16 field_24;
            u16 field_26;
        } movie;
        struct {
            u32 field_8;
            u16 stageIdx;
            u16 id;
            u32 subId;
            u16 field_14;
            u16 field_16;
            u32 field_18;
            u16 field_1c;
            u16 field_1e;
            u8  field_20[8];
        } audio;
    } data;
} FsCdfStream;
STATIC_ASSERT_SIZEOF(FsCdfStream, 0x28);

/// An entry of the folder list, as stored in a .CDF file.
typedef struct _FsCdfFolderListEntry {
    u32 id;   // Folder id.
    u32 size; // Folder size.
} FsCdfFolderListEntry;

/// A list of folders found in the header section of a .CDF file.
typedef struct _FsCdfFolderList {
    FsCdfFolderListEntry entries[0x100]; // List of folders. Zero padded.
} FsCdfFolderList;
STATIC_ASSERT_SIZEOF(FsCdfFolderList, FS_SECTOR_BYTE_SIZE);

typedef struct _FsCdfFolder {
    u32 id;     // Folder id.
    u32 offset; // Offset from the beginning of the file.
} FsCdfFolder;

typedef struct _FsCdfFile {
    u32 id;     // File id.
    u32 offset; // Offset from the beginning of the folder.
} FsCdfFile;

typedef struct _FsCdfFileSmall {
    u16 id;     // File id.
    u16 offset; // Offset from the beginning of the folder.
} FsCdfFileSmall;

typedef struct _FsCdfChunkHeader {
    u8    type;     // Type of data stored in the chunk.
    u8    endFlag;  // Flag indicating whether the chunk is the last in the file.
    u16   field_2;  // Unknown.
    u32   size;     // Chunk size (sector count for CD reads).
    void* loadAddr; // Address to where the chunk must be loaded. Non-NULL for pkgs.
    u32   padding;  // Padding bytes. Are always 0 in the USA version.
} FsCdfChunkHeader;
STATIC_ASSERT_SIZEOF(FsCdfChunkHeader, 0x10);

/// Start of a file chunk in a CDF file.
///
/// All chunks start with a header, followed with the actual chunk data.
/// A chunk is always padded to the next multiple of a cd sector size.
typedef struct _FsCdfChunk {
    FsCdfChunkHeader header;
    union {
        u8  bytes[FS_SECTOR_BYTE_SIZE - sizeof(FsCdfChunkHeader)];
        u32 words[FS_SECTOR_WORD_SIZE - (sizeof(FsCdfChunkHeader) / 4)];
    } data;
} FsCdfChunk;
STATIC_ASSERT_SIZEOF(FsCdfChunk, FS_SECTOR_BYTE_SIZE);

/// Contents of a CD sector.
typedef union _FsSector {
    u8               bytes[FS_SECTOR_BYTE_SIZE];
    u32              words[FS_SECTOR_WORD_SIZE];
    FsCdfFolderList  folderList;
    FsCdfChunkHeader chunk;
} FsSector;
STATIC_ASSERT_SIZEOF(FsSector, FS_SECTOR_BYTE_SIZE);

// =============================================================================
// CD command queue (producers: game / boot; consumer: C37C)
// =============================================================================

/// One slot in the CD load command ring (`CdCmd_Enqueue`).
///
/// File identity is packed as base-100 digits (`idB2*10000 + idB1*100 + idB0`).
/// `cmd` selects the operation (0x21 load file, 0x54 mount stage, 0x55 parse HED).
typedef struct _CdCmdEntry {
    u8 idB0;   // ones digit / a2[0]
    u8 idB1;   // hundreds digit / a2[1]
    u8 idB2;   // ×10000 / category / a2[2]
    u8 idB3;   // a2[3]
    u8 cmd;    // command opcode
    u8 param0; // a1[3]
    u8 param1; // a1[2]
    u8 param2; // a1[0]
} CdCmdEntry;
STATIC_ASSERT_SIZEOF(CdCmdEntry, 0x8);

/// Stream/load descriptor pointed to by `CdCmdQueue.field_190`.
typedef struct _CdCmd190 {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   field_3; // buffer slot select for CdCmd_SetupMdecBuffers
    /* 0x04 */ s32  field_4;
    /* 0x08 */ byte unknown_8[0xC];
    /* 0x14 */ u16  field_14;
    /* 0x16 */ u16  field_16;
    /* 0x18 */ byte unknown_18[0x2];
    /* 0x1A */ u16  field_1A; // VLC table slot (0=malloc, 1..3=shared buffers)
    /* 0x1C */ u16  field_1C; // byte length used to skip extra CD sectors
    /* 0x1E */ u16  field_1E; // malloc size when field_3 == 1
} CdCmd190;
STATIC_ASSERT_SIZEOF(CdCmd190, 0x20);

/// Per-slot decode entry at `CdCmdQueue.field_58` (5 entries, stride 0x3C).
/// Used by `Mdec_ResolveStreamBuffer` to map a stream id to a base buffer + byte offset.
/// Also holds up to 3 work-list / image-chunk offsets used by `Mdec_ProcessDecode`.
typedef struct _CdCmd58Entry {
    /* 0x00 */ s32  field_0;     // byte offset added to the resolved base buffer
    /* 0x04 */ s32  field_4[3];  // FsWorkEntry offsets (Fs_CopyWorkEntries path)
    /* 0x10 */ s32  field_10[3]; // FsImageChunk offsets (Fs_LoadImageChunk path)
    /* 0x1C */ s32  field_1C;    // memcpy source offset for Mem_CopyUnaligned
    /* 0x20 */ byte pad_20[0x4];
    /* 0x24 */ s16  field_24[3]; // non-zero → Fs_ChunkMode=2 / D5B498_8006C233=-8
    /* 0x2A */ s16  field_2A[3]; // non-zero → Fs_ChunkMode=2 / D5B498_8006C234=-3
    /* 0x30 */ byte pad_30[0x2];
    /* 0x32 */ s16  field_32;    // stream id matched against GameSession.field_4
    /* 0x34 */ s16  field_34;    // buffer-select kind (0..4) for the switch in Mdec_ResolveStreamBuffer
    /* 0x36 */ byte pad_36[0x2];
    /* 0x38 */ u16  field_38;    // memcpy byte count for Mem_CopyUnaligned
    /* 0x3A */ byte pad_3A[0x2];
} CdCmd58Entry;
STATIC_ASSERT_SIZEOF(CdCmd58Entry, 0x3C);

/// Global CD / asset load command queue (`CdCmd_Queue`).
typedef struct _CdCmdQueue {
    CdCmdEntry   entries[8];
    CdCmdEntry   field_40;
    s32          field_48; // last CD position from CdPosToInt
    s8           field_4c;
    byte         unknown_4d[0x3];
    CdCmdEntry   field_50;    // replace-slot used by CdCmd_EnqueueReplace (cmd at 0x54)
    CdCmd58Entry field_58[5]; // 0x58..0x183 — stream decode slot table
    void*        field_184;   // 0x184 — aux buffer (malloc of field_188)
    s32          field_188;   // 0x188 — size for field_184 malloc
    u16*         field_18C;   // 0x18C — VLC / DCT table buffer
    CdCmd190*    field_190;   // 0x190
    void*        field_194;   // 0x194
    void*        field_198;   // 0x198 — base buffer for field_58 kind 4
    u32*         field_19C;   // 0x19C — copy of field_1A4; timing table cursor
    u32          field_1A0;   // 0x1A0 — timing accumulator (GameMain_Loop)
    void*        field_1A4;   // 0x1A4 — secondary image/stream buffer
    s32          field_1A8;   // 0x1A8 — copied to Gp_LcgState by Gp_RestoreStreamRng
    u32          field_1AC;   // 0x1AC — srand seed restored by Gp_RestoreStreamRng
    byte         unknown_1B0[0x18];
    u16          writeIdx;    // 0x1C8 — next free slot (enqueue)
    u16          readIdx;     // 0x1CA — slot being executed
    byte         unknown_1cc[0x4];
    u16          step;        // 0x1D0 — sub-state of current command
    u16          field_1d2;
    u16          field_1d4;
    u16          field_1D6; // state for CdCmd_SeekL
    byte         unknown_1d8[0x6];
    u16          field_1DE; // state for CdCmd_PausePoll
    u16          field_1E0;
    u16          field_1E2;
    u16          field_1E4;
    u16          field_1E6;
    u16          field_1E8;
    u16          field_1EA;
    u16          field_1EC; // MDEC out strip active (cleared by DecDCTout callback)
    u16          field_1EE; // 0x1EE — stream slot latched by the plaza cutscene tasks
    u16          field_1F0;
    u16          field_1F2;
    u16          field_1F4;
    u16          field_1F6;
    u16          field_1F8; // 0x1F8 — plaza ambience state (0/1 emitters, 2 fade)
    u16          field_1FA;
    u16          field_1fc;
    u8           field_1FE; // load status (0xFF = idle/done in several paths)
    u8           field_1FF;
    u16          field_200;
    u16          field_202;
    u16          field_204;
    byte         unknown_206[0x4];
    u16          field_20A;
    byte         unknown_20C[0x2];
    s16          field_20E;
    u16          field_210; // 0x210 — set when Stream_FindSlot succeeds (Gp_ViewBeginLoad)
    u16          field_212;
    u16          field_214;
    u16          field_216; // 0x216 — non-zero enables buffer setup in CdCmd_SetupMdecBuffers
    u16          field_218; // 0x218 — non-zero blocks Mdec_ResolveStreamBuffer success path
    s16          field_21A;
    u16          field_21C; // image transfer mode for Display_LoadImageStrips (0 / 1)
    u16          field_21E; // 0x21E — DecDCTvlcBuild done flag
    byte         unknown_220[0x2];
    s16          field_222;
    u16          field_224;
    u16          field_226; // sub-state for CdCmd_RecoverDisk disk recovery
    u16          field_228;
    u16          field_22A; // DecDCTin mode for Mdec_DecodeToVram
    u16          field_22C;
    u16          field_22E;
    u16          field_230;
    u16          field_232;
    s16          field_234;
    s16          field_236;
    s16          field_238; // 0x238 — non-zero clears field_18C in CdCmd_SetupMdecBuffers
    s16          field_23A; // 0x23A — set to 1 by Gp_RestoreStreamRng
    byte         unknown_23C[0x2];
    s16          field_23E; // MoveImage vs ClearImage path for Mdec_DecodeToVram
    s16          field_240; // non-zero enables CD timing wait (GameMain_Loop)
    s16          field_242;
    u16          field_244;
    u16          field_246;
    u16          field_248;
    u16          field_24A;
    byte         unknown_24C[0x6];
    s16          busy; // 0x252 — non-zero while a blocking load is active
} CdCmdQueue;
STATIC_ASSERT_SIZEOF(CdCmdQueue, 0x254);

// =============================================================================
// Runtime FS state helpers (still poorly named internals)
// =============================================================================

/// 8-byte work entry used by the FS load path (`Fs_WorkEntries`).
typedef struct _FsWorkEntry {
    u16 field_0;
    u16 field_2;
    u32 field_4;
} FsWorkEntry;
STATIC_ASSERT_SIZEOF(FsWorkEntry, 0x8);

/// Per-folder slot cleared by `Fs_PrepareFolderLoad` (50 entries, parallel to
/// `Fs_FolderTable`). Only the first byte is written by the init path.
typedef struct _FsFolderSlot {
    u8  field_0;
    u8  pad_1[3];
    s32 field_4;
} FsFolderSlot;
STATIC_ASSERT_SIZEOF(FsFolderSlot, 0x8);

/// Small FS control block cleared at the start of `Fs_PrepareFolderLoad`.
typedef struct _FsUnkADE8 {
    u16 field_0;
    u16 field_2;
    u16 field_4;
    u16 pad_6;
    u32 field_8;
} FsUnkADE8;
STATIC_ASSERT_SIZEOF(FsUnkADE8, 0xC);

/// Compact image/load params stored during FS setup (`Fs_LoadParams`).
typedef struct _FsLoadParams {
    byte unknown_0[0x2];
    u8   field_2;
    u8   field_3;
} FsLoadParams;
STATIC_ASSERT_SIZEOF(FsLoadParams, 0x4);

/// Large dual-buffer image area pointed to by `Fs_ImgBuffers`.
typedef struct _FsImgBuffers {
    u32 buffers[20][1920];
} FsImgBuffers;
STATIC_ASSERT_SIZEOF(FsImgBuffers, 0x25800);

/// On-disk / in-sector image chunk header used by `Fs_LoadImageChunk`.
/// Fields at +4/+6 are height then width (swapped relative to RECT).
typedef struct _FsImageChunk {
    u16 x;
    u16 y;
    u16 h;
    u16 w;
    u8  pad[8];
    // pixel data follows at offset 0x10
} FsImageChunk;
STATIC_ASSERT_SIZEOF(FsImageChunk, 0x10);

// Forward decl for boot task callback (full type in task.h).
struct _Task;

// =============================================================================
// Functions — CD command queue
// =============================================================================

s32         CdCmd_Enqueue(s32 cmd, u8* paramA, u8* paramB);
void        CdCmd_EnqueueReplace(s32 cmd, u8* paramA, u8* paramB);
void        CdCmd_ClearQueue(void);
s32         CdCmd_CommitReplace(void);
s32         CdCmd_DropPending(void);
u16         CdCmd_IsIdle(void);
u16         CdCmd_IsSlotEmpty(s16 slot);
s16         CdCmd_GetStreamMode(void);
void        CdCmd_SetBusy(void);
void        CdCmd_ClearBusy(void);
void        CdCmd_ResetRing(void);
void        CdCmd_ResetEntryIter(void);
CdCmdEntry* CdCmd_NextEntry(void);
void        CdCmd_LoadActiveEntry(void);
void        CdCmd_EnqueueUnlessStream(s32 cmd, u8* paramA, u8* paramB);
void        CdCmd_AdvanceRead(void);
void        CdCmd_Dispatch(void);
s32         CdCmd_ActivatePhase1(void);
u16         CdCmd_ActivatePhase2(void);
void        CdCmd_ProcessPhase1(void);
void        CdCmd_ProcessPhase2(void);
void        CdCmd_HandleFileLoad(void);
void        CdCmd_HandleMount(void);
void        CdCmd_HandleStreamDecode(void);
u16         CdCmd_EnqueueFollowUp(void);
void        CdCmd_RequestVlcRebuild(void);
s32         CdCmd_SeekL(u8* loc);
s32         CdCmd_PausePoll(void);
s16         CdCmd_RecoverDisk(void);
s32         CdCmd_PollStatus(s32 arg0, s32 arg1);
s32         CdCmd_StopMdec(s32 clearFb);
s32         CdCmd_GetOverlayStatus(void);
void        CdCmd_EnqueueOverlay81(void);
void        CdCmd_EnqueueReplaceOverlay81(void);
void        CdCmd_EnqueueOverlay82(void);
void        CdCmd_EnqueueReplaceOverlay82(void);
void        func_8001D580(void);
void        CdCmd_CancelReplaceAndActivate(void);
void*       CdCmd_SetupMdecBuffers(void);
void        CdCmd_BuildVlcIfStream(void);
void        CdCmd_SelectMdecBuffer(void);
void        CdCmd_StartOverlay(u16 arg0, u16 arg1, u16 arg2);
void        CdCmd_EnqueueLoadFile(s32 arg0, s32 arg1, s32 arg2);
void        CdCmd_StepVlcRebuild(void);

extern u16 CdCmd_EntryIter;

// =============================================================================
// Functions — filesystem / CD (src/main/fs.c)
// =============================================================================

void Fs_CdReadyCb(u8 status, u8* result);
u8   Fs_ProcessChunkHeader(void);
u8   Fs_ProcessChunkData(void);

/// Resumable LZ/bit-stream unpack for CD chunk payloads (handwritten hasm).
/// Uses globals Fs_ChunkReadPtr / Fs_ChunkWritePtr / D5B498_8006D748; may suspend
/// mid-stream when the sector buffer ends (resume jtbl in same TU).
void Fs_DecompressChunk(void);
/// Non-resumable LZ unpack for image strips before LoadImage2 (handwritten hasm).
void Fs_DecompressImage(void);

void Fs_InitStage0TablesCb(u8 status, u8* result);
void Fs_InitFolderTable(s32 stageIdx);
void Fs_SelectStage(s32 stageIdx);
void Fs_InitStage0Tables(void);
void Fs_ClearDiskError(void);
u8   Fs_WaitDiskSwap(void);
void Fs_ReadSectorEx(s32 sector, s32 endSector, u8* dest, u8 mode);
void Fs_ReadSector(s32 sector);
void Fs_WaitDiskReset(s8 withSectHdr);
void Fs_SeekToPos(s32 sector);
void Fs_SeekToPosCb(u8 status, u8* result);
void Fs_ContinueDrawing(u_long* ot);
void Fs_StopCd(void);
bool Fs_StageCdfIsAvailable(u32 stageIdx);
void Fs_ScanIsoDirectory(s32 mode);

/// Load an image chunk into VRAM (BreakDraw / LoadImage2 path).
/// `retryNonzero` disables timeout aborts when non-zero.
s32 Fs_LoadImageStrip(s32 arg0);
s32 Fs_LoadImageChunk(FsImageChunk* img, u8 retryNonzero);

/// Copy a terminated FsWorkEntry list into Fs_WorkEntries and set up
/// Fs_ImageRect / load state for the following image transfer.
void Fs_CopyWorkEntries(FsWorkEntry* arg0);

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
void Boot_LoadInitialFile(struct _Task* task);

void Fs_ResetBootLoadState(void);
void Fs_BeginBootLoad(u8* arg0, s16 arg1);
void Fs_EnsureBootLoadStarted(void);
void Fs_StepBootImage(void);
void Fs_RetryReadN(void);
void Fs_ReadNSyncCb(u8 status, u8* result);
void Fs_ReadNReadyCb(u8 status, u8* result);
void Fs_OnCdError(u8 arg0);
void Fs_CheckReadTimeout(void);
u8*  Fs_GetChunkPayload(void);

/// Boot-image / CD load setup (src/main/bootload.c).
void Fs_SetupBootLoad(void);
void Fs_BootImageMachine(void* primaryTim, void* secondaryTim);

/// CD ready callback used while streaming bank data (src/main/cdvol.c).
void Fs_StreamReadyCb(u8 status, u8* result);

// =============================================================================
// Functions — CD volume / mix (src/main/cdvol.c)
// =============================================================================

void CdVol_SetMixMode(s32 stereo);
u8   CdVol_GetMixMode(void);
void CdVol_CacheFromSpu(void);
void CdVol_RegisterCallbacks(void);
void CdVol_ClearCallbackSlot(void);
s32  Cd_Flush(void);
s32  CdVol_Get(void);
void CdVol_Set(s32 level /* 0..0x7F */);
void CdVol_ApplyFromTable(u16 index);
s32  CdVol_StepDown(void);
s32  Cd_InitStateMachine(u32* stateFlags);

// =============================================================================
// Globals — CD command queue
// =============================================================================

extern CdCmdQueue CdCmd_Queue;

// =============================================================================
// Globals — FS runtime tables / CD state (bss unit `fs`)
// =============================================================================

// Load / seek control
extern s32 Fs_Stage0HedSector;
extern s32 Fs_SeekSector;
extern s32 Fs_ReqSector;
extern s32 Fs_CurrSector;
extern s32 Fs_VBlank;
extern u8  Fs_CdOpStatus;
extern u8  Fs_LoadPhase;
extern u8  Fs_Streaming;
extern u8  Fs_ChunkMode;
extern u8  Fs_ChunkEndFlag;
extern u8* Fs_ChunkWritePtr;
extern s32 Fs_ChunkEndSector;
extern u8  Fs_CdErrorCount;

// STAGE0.HED-derived tables
extern u16            Fs_FileOffsetsCat0[0x30];
extern u16            Fs_FileOffsetsCat5[0x40];
extern u32            Fs_FileOffsetsCat90[0x8];
extern FsCdfFileSmall Fs_FileTableCat1[0x3c];
extern u8             Fs_FileTableCat1Len;
extern FsCdfFileSmall Fs_FileTableCat2[0x160];
extern u16            Fs_FileTableCat2Len;
extern FsCdfFileSmall Fs_FileTableCat3[0x1e];
extern u8             Fs_FileTableCat3Len;
extern FsCdfFileSmall Fs_FileTableCat4[0x46];
extern u8             Fs_FileTableCat4Len;
extern FsCdfFile      Fs_FileTable[0x10e];
extern u16            Fs_FileTableLen;
extern FsCdfFolder    Fs_FolderTable[50];
extern u16            Fs_FolderTableLen;
extern s32            Fs_StageCdfSectors[FS_CDF_STAGE_COUNT];
extern FsCdfStream    Fs_Streams[0xa];

/// Absolute sector offsets for folder-local file ids (indexed by file id).
/// Filled by `Fs_BuildFolderTables` from the folder file list in `Fs_CdSector`.
extern s32 D_8006C158[0x33];

// Buffers / media
extern FsSector      Fs_CdSector;
extern RECT          Fs_ImageRect;
extern SpuCommonAttr Fs_SpuAttr;

// Boot load-buffer pointer (data unit `boot_loadbuf`)
extern FsImgBuffers* Fs_ImgBuffers;

// Still-unlabeled FS bss (same segment; keep address names until understood)
/// Per-slot image-load status bytes (indexed by `D5B498_8006ADF4`).
extern u8    D_8006C4C8[0xC];
extern s16   Fs_BootLoadSlot;
extern u16   Fs_BootLoadPhase;
extern u16   D5B498_8006AC9C;
extern u8    D_8006AC9E;
extern u8    D_8006AC9F;
extern s16   D_8006ACA0;
extern s16   D_8006ACA2;
extern s16   D_8006ACA4;
extern s16   D_8006ACA6;
extern s16   D_8006ACA8;
extern void* Fs_BootTimSecondary;
extern void* Fs_BootTimPrimary;
// Fade/clear color; written as halfword, often re-read as byte for TILE RGB.
extern volatile s16 D_8006ACB4;
extern FsLoadParams Fs_LoadParams;
extern s16          D5B498_8006ACC0;
extern u8           D5B498_8006ACC8;
extern u16          D5B498_8006ACD4;
extern FsWorkEntry  Fs_WorkEntries[0x1F];
extern u8           D5B498_8006ADE0;
extern u8           D5B498_8006ADE1;
extern u8           D_8006ADE2;
extern FsUnkADE8    D_8006ADE8;
extern s32          D_8006D868;
extern s32          D_8006D864;
extern s32          D_8006D860;
extern u8           D_8006CCD8[];
extern s32          D_8006ADF8;
extern u8           D5B498_8006ADF4;
/// ISO root scan: LBA of the first `.STR` (stream) file, plus a sibling word.
typedef struct {
    s32 sector;
    s32 field_4;
} FsStrInfo;
extern FsStrInfo    D_8006AC30;
extern u8*          D_8006C4D4;
extern FsFolderSlot D_8006C338[50];
extern u8*          Fs_ChunkReadPtr;
extern u8           D5B498_8006C233;
extern u8           D5B498_8006C234;
extern u8           D5B498_8006D4E0[0x10];
extern u16          D5B498_8006D748;
extern void*        D5B498_8006D850;
extern u16          D5B498_8006D858;
extern u16          D5B498_8006D85A;
extern u_long       D5B498_8006D870[0x460];
extern u16          D5B498_8006EA1A;
extern u16          D5B498_8006EBB0;
extern s16          D5B498_8006EBF0;

// Args used by Fs_OnCdError
#define FS_ERROR_SOFT 0x0
#define FS_ERROR_HARD 0x2

// Boot-image TIM / stream tables selected by Fs_SelectLoadHandlers*
extern u8 D_80062934[];
extern u8 D_80062954[];
extern u8 D_800629B0[];
extern u8 D_800629D0[];
extern u8 D_80062A24[];
extern u8 D_80062A44[];
extern u8 D_80062AB4[];
extern u8 D_80062AD4[];
extern u8 D_80062B30[];
extern u8 D_80062B50[];
extern u8 D_80062BA4[];
extern u8 D_80062BC4[];
extern u8 D_80062C20[];
extern u8 D_80062C40[];
extern u8 D_80062C94[];
extern u8 D_80062CB4[];
extern u8 D_80062D08[];
extern u8 D_80062DB0[];
extern u8 D_80062E04[];
extern u8 D_80062E50[];
extern u8 D_80062EA0[];
extern u8 D_80062EEC[];
extern u8 D_80062F34[];
extern u8 D_80062F80[];
extern u8 D_80062FD0[];
extern u8 D_80063018[];
extern u8 D_80063068[];
extern u8 D_800630B0[];

/// VLC / MDEC strip bases used by CdCmd_SetupMdecBuffers.
extern u16*          D_8005C36C;
extern u16*          D_8005C370;
extern u16*          D_8005C374;
extern u8*           Mdec_DecodeBase; // resolved decode base (Mdec_ResolveStreamBuffer)
extern CdCmd58Entry* Stage_CdEntry;   // matched CdCmd_Queue.field_58 entry
extern u16           D_8007A35C;

#endif
