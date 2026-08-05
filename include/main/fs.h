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

/// Global CD / asset load command queue (`CdCmd_Queue`).
typedef struct _CdCmdQueue {
    CdCmdEntry entries[8];
    CdCmdEntry field_40;
    s32        field_48; // last CD position from CdPosToInt
    s8         field_4c;
    byte       unknown_4d[0x3];
    CdCmdEntry field_50; // replace-slot used by CdCmd_EnqueueReplace (cmd at 0x54)
    byte       unknown_58[0x170];
    u16        writeIdx; // 0x1C8 — next free slot (enqueue)
    u16        readIdx;  // 0x1CA — slot being executed
    byte       unknown_1cc[0x4];
    u16        step;     // 0x1D0 — sub-state of current command
    u16        field_1d2;
    u16        field_1d4;
    u16        field_1D6; // state for func_8001DF34
    byte       unknown_1d8[0x6];
    u16        field_1DE; // state for func_8001E2D4
    u16        field_1E0;
    u16        field_1E2;
    u16        field_1E4;
    s16        field_1E6;
    u16        field_1E8;
    s16        field_1EA;
    u16        field_1EC; // MDEC out strip active (cleared by DecDCTout callback)
    byte       unknown_1EE[0x6];
    u16        field_1F4;
    u16        field_1F6;
    byte       unknown_1F8[0x2];
    u16        field_1FA;
    u16        field_1fc;
    u8         field_1FE; // load status (0xFF = idle/done in several paths)
    u8         field_1FF;
    u16        field_200;
    u16        field_202;
    u16        field_204;
    byte       unknown_206[0x4];
    s16        field_20A;
    byte       unknown_20C[0x2];
    s16        field_20E;
    byte       unknown_210[0x2];
    u16        field_212;
    u16        field_214;
    byte       unknown_216[0x4];
    s16        field_21A;
    byte       unknown_21C[0x6];
    s16        field_222;
    u16        field_224;
    u16        field_226; // sub-state for func_8001E57C disk recovery
    u16        field_228;
    byte       unknown_22a[0xA];
    s16        field_234;
    s16        field_236;
    byte       unknown_238[0xA];
    s16        field_242;
    u16        field_244;
    byte       unknown_246[0x2];
    u16        field_248;
    u16        field_24A;
    byte       unknown_24C[0x6];
    s16        busy; // 0x252 — non-zero while a blocking load is active
} CdCmdQueue;
STATIC_ASSERT_SIZEOF(CdCmdQueue, 0x254);

// =============================================================================
// Runtime FS state helpers (still poorly named internals)
// =============================================================================

/// 8-byte work entry used by the FS load path (`D5B498_8006ACE8`).
typedef struct _FsWorkEntry {
    u16 field_0;
    u16 field_2;
    u32 field_4;
} FsWorkEntry;
STATIC_ASSERT_SIZEOF(FsWorkEntry, 0x8);

/// Compact image/load params stored during FS setup (`D5B498_8006ACB8`).
typedef struct _FsLoadParams {
    byte unknown_0[0x2];
    u8   field_2;
    u8   field_3;
} FsLoadParams;
STATIC_ASSERT_SIZEOF(FsLoadParams, 0x4);

/// Large dual-buffer image area pointed to by `D4CB64_ImgBuffers`.
typedef struct _FsImgBuffers {
    u32 buffers[20][1920];
} FsImgBuffers;
STATIC_ASSERT_SIZEOF(FsImgBuffers, 0x25800);

/// On-disk / in-sector image chunk header used by `func_800246B0`.
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

// Forward decl for boot task callback (full type in game.h).
struct _Task;

// =============================================================================
// Functions — CD command queue
// =============================================================================

s32  CdCmd_Enqueue(s32 cmd, u8* paramA, u8* paramB);
void CdCmd_EnqueueReplace(s32 cmd, u8* paramA, u8* paramB);
void CdCmd_ClearQueue(void);

// =============================================================================
// Functions — filesystem / CD (src/main/fs.c)
// =============================================================================

void Fs_CdReadyCb(u8 status, u8* result);
u8   Fs_ProcessChunkHeader(void);
u8   Fs_ProcessChunkData(void);
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
s32 func_800246B0(FsImageChunk* img, u8 retryNonzero);

/// Look up a packed file id and start a CD load (unmatched).
void Fs_LoadFile(u8* req, s32 a1, s32 a2, s32 a3);

/// Boot path: scan ISO, parse HED, load initial CDF file (file id 1).
void Boot_LoadInitialFile(struct _Task* task);

// Still-unknown stubs in fs.c (legacy names until matched).
void F12D18_80022518(void);
void F12D18_8002252C(u8* arg0, s16 arg1);
void F12D18_80022598(void);
void F12D18_800225D4(void);
void F12D18_80024EC0(void);
void F12D18_80025580(u8 status, u8* result);
void F12D18_8002563C(u8 status, u8* result);
void F12D18_800256F4(u8 arg0);
void F12D18_800257B0(void);
u8*  func_800257A4(void);

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

// Buffers / media
extern FsSector      Fs_CdSector;
extern RECT          Fs_ImageRect;
extern SpuCommonAttr Fs_SpuAttr;

// Boot load-buffer pointer (data unit `boot_loadbuf`)
extern FsImgBuffers* D4CB64_ImgBuffers;

// Still-unlabeled FS bss (same segment; keep address names until understood)
/// Per-slot image-load status bytes (indexed by `D5B498_8006ADF4`).
extern u8           D_8006C4C8[0xC];
extern s16          D5B498_8006AC98;
extern u16          D5B498_8006AC9A;
extern u16          D5B498_8006AC9C;
extern void*        D5B498_8006ACAC;
extern void*        D5B498_8006ACB0;
extern FsLoadParams D5B498_8006ACB8;
extern s16          D5B498_8006ACC0;
extern u8           D5B498_8006ACC8;
extern u16          D5B498_8006ACD4;
extern FsWorkEntry  D5B498_8006ACE8[0x1F];
extern u8           D5B498_8006ADE0;
extern u8           D5B498_8006ADE1;
extern u8           D5B498_8006ADF4;
extern u8*          D5B498_8006C22C;
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

// Args used by F12D18_800256F4
#define FS_ERROR_SOFT 0x0
#define FS_ERROR_HARD 0x2

#endif
