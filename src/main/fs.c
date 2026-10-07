#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/kernel.h>
#include <psyq/libapi.h>
#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/strings.h>

#include "common.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs_types.h"
#include "fs_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "sound.h"
#include "stream.h"
#include "main/stream_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

/// Where one folder of a stage CDF starts, as the loader keeps it.
///
/// The CDF's own folder list, `FsCdfFolderList`, gives each folder's length
/// only. Reading it accumulates those lengths into one of these records per
/// folder, so a load can find a folder by number and seek to its first sector.
/// The sectors a folder's file list and stream table give count from that
/// first sector, and are rebased by the same offset.
typedef struct {
    u32 folderId;     // Disc folder number, as in `FsCdfFolderListEntry`
    u32 sectorOffset; // CD sectors from the start of the stage CDF to the folder's first sector
} _FsCdfFolder;
STATIC_ASSERT_SIZEOF(_FsCdfFolder, 0x8);

/// A compact `STAGE0.HED` file entry, kept in the tables for file categories 1-4.
///
/// The HED lists every STAGE0 file as a pair of words, file id and sector
/// offset. Ids whose category (`id / 10000`) is 1-4 are stored at half width,
/// one table per category, and looked up by the request's
/// `fileIdHundreds * 100 + fileIndex`; larger ids keep the full `FsCdfFile`.
typedef struct {
    u16 idInCategory; // File id less its category's multiple of 10000
    u16 sectorOffset; // Sector of the file relative to the start of `STAGE0.CDF`
} _FsStage0CategoryFile;
STATIC_ASSERT_SIZEOF(_FsStage0CategoryFile, 0x4);

/// One entry in the directory of a CDF resource bundle (chunk opcode 4).
///
/// Fifty entries open the payload of the bundle's first sector, one per
/// `FsResourceSlot`. The payload sectors that follow stream contiguously from
/// the chunk's load address, and `destination` is where each resource lands in
/// that stream. The loader publishes only `kind` and `destination`, and arms
/// `Fs_LoadRedirect` from an entry with a `redirectDestination`. `byteSize` is
/// not read; retail bundles pack resources at 16-byte aligned destinations and
/// leave the redirect pair zero.
typedef struct {
    u8    kind;                  // Resource kind (0 none, 2 image, 3 untyped data)
    u8    field_1;               // Unread; zero in every retail bundle, role unproven
    u16   sectorsBeforeRedirect; // Payload sectors written before the stream moves to `redirectDestination`
    u32   byteSize;              // Resource length in bytes
    void* destination;           // Absolute RAM address of the resource
    u8*   redirectDestination;   // Write pointer for the remaining payload sectors; NULL to stay contiguous
} _FsCdfResourceEntry;
STATIC_ASSERT_SIZEOF(_FsCdfResourceEntry, 0x10);

/// A pending jump of a resource bundle's payload stream to a second RAM address.
///
/// A bundle's payload is normally copied sector by sector to one contiguous
/// address. A directory entry with a `redirectDestination` arms this record;
/// once `sectorsRead` payload sectors reach `sectorsBeforeRedirect`, the write
/// pointer moves to `destination` and the remaining sectors land there. Only
/// one redirect is held: the last such entry in the directory wins. The record
/// is disarmed only when a folder load is prepared, so a later bundle in the
/// same load re-uses an earlier bundle's redirect unless it arms its own.
typedef struct {
    u16 enabled;               // Nonzero once a bundle entry has armed a redirect
    u16 sectorsRead;           // Payload sectors streamed since the bundle directory
    u16 sectorsBeforeRedirect; // Value of `sectorsRead` at which the write pointer jumps
    u8* destination;           // RAM address receiving the payload sectors after the jump
} _FsLoadRedirect;
STATIC_ASSERT_SIZEOF(_FsLoadRedirect, 0xC);

/// Canary value at the end of the STAGE0.HED header.
#define FS_CDF_STAGE0_CANARY -1

/// Canary value at the end of the folder list.
#define FS_CDF_FOLDER_CANARY 0

/// Idle / finished value for `Fs_CdOpStatus`.
#define FS_CD_STATUS_IDLE -1

// Args used by _fsHandleCdError
#define FS_ERROR_SOFT 0x0

#define FS_ERROR_HARD 0x2

/// Filesystem operation states used by the seek/read recovery paths.
enum {
    FILE_SYSTEM_CD_OPERATION_PENDING         = 0,
    FILE_SYSTEM_CD_OPERATION_RESUME_CHUNK    = 0x40,
    FILE_SYSTEM_CD_OPERATION_RESUMING_CHUNK  = 0x41,
    FILE_SYSTEM_CD_OPERATION_RESTART_REQUEST = 0x80,
};

/// Chunk-load policies, distinct from the queued request's loadMode encoding.
enum {
    FILE_SYSTEM_CHUNK_LOAD_NORMAL          = 0,
    FILE_SYSTEM_CHUNK_LOAD_INIT_SOUND      = 1,
    FILE_SYSTEM_CHUNK_LOAD_RELOCATE_IMAGES = 2,
    FILE_SYSTEM_CHUNK_LOAD_SKIP_BACKGROUND = 3,
    FILE_SYSTEM_CHUNK_LOAD_IMAGES_ONLY     = 4,
    FILE_SYSTEM_CHUNK_LOAD_SKIP_SOUND      = 5,
};

/* Define BSS before API headers to preserve first-declaration order. */
static u8 D5B498_8006ACC8;

static s32 Fs_Stage0HedSector;

static s32 Fs_SeekSector;

static u16 D5B498_8006ACD4;

static RECT Fs_ImageRect;

/// Unreferenced.
static u8 D_8006ACE0[8];

static FsImageColumn Fs_WorkEntries[0x1F];

static u8 D5B498_8006ADE0;

static u8 D5B498_8006ADE1;

static u8 D_8006ADE2;

static _FsLoadRedirect Fs_LoadRedirect;

static u8 D5B498_8006ADF4;

static s32 D_8006ADF8;

/// Unreferenced.
static u8 D_8006AE00[8];

static u16 Fs_FileOffsetsCat0[0x30];

static u16 Fs_FileOffsetsCat5[0x40];

static _FsStage0CategoryFile Fs_FileTableCat3[0x1e];

static u8 Fs_FileTableCat3Len;

static _FsStage0CategoryFile Fs_FileTableCat4[0x46];

static u8 Fs_FileTableCat4Len;

static _FsStage0CategoryFile Fs_FileTableCat1[0x3c];

static u8 Fs_FileTableCat1Len;

/// Unreferenced.
static u8 D_8006B180[8];

static _FsStage0CategoryFile Fs_FileTableCat2[0x160];

static u16 Fs_FileTableCat2Len;

/// Unreferenced.
static u8 D_8006B710[8];

static FsCdfFile Fs_FileTable[0x10e];

static u16 Fs_FileTableLen;

/// Unreferenced.
static u8 D_8006BF90[8];

static u32 Fs_FileOffsetsCat90[0x8];

static _FsCdfFolder Fs_FolderTable[50];

static u16 Fs_FolderTableLen;

/// Unreferenced.
static u8 D_8006C150[8];

/// Absolute sector offsets for folder-local file ids (indexed by file id).
/// Filled by `fsBuildFolderTables` from the folder file list in `Fs_CdSector`.
static s32 D_8006C158[0x33];

s32 Fs_ReqSector;

u8 Fs_CdOpStatus;

u8* Fs_ChunkReadPtr;

static u8 Fs_LoadPhase;

static u8 Fs_Streaming;

u8 Fs_ChunkMode;

s8 D5B498_8006C233;

s8 D5B498_8006C234;

/// Referenced only by the table at the start of the image.
u8 D_8006C238[0x100];

FsResourceSlot D_8006C338[50];

/// Per-slot image-load status bytes (indexed by `D5B498_8006ADF4`).
static u8 D_8006C4C8[0xC];

u8* D_8006C4D4;

FsSector Fs_CdSector;

static u8 D_8006CCD8[0x800];

u8* Fs_ChunkWritePtr;

static u8 D5B498_8006D4E0[0x10];

StreamSlot Stream_Slots[15];

u16 D5B498_8006D748;

/// Referenced only by the table at the start of the image.
u8 D_8006D750[0x100];

void* D5B498_8006D850;

static s32 Fs_ChunkEndSector;

u16 D5B498_8006D858;

u16 D5B498_8006D85A;

/// Unreferenced.
static s32 D_8006D85C;

/// Bytes produced in each streaming destination; -1 marks a pending destination.
s32 Fs_ChunkOutputSizes[3];

static u8 Fs_ChunkEndFlag;

static u_long D5B498_8006D870[0x460];

s32 Fs_StageCdfSectors[FS_CDF_STAGE_COUNT];

s16 D_8006EA08;

s16 D_8006EA0A;

s32 D_8006EA0C;

s32 Fs_VBlank;

static s32 Fs_CurrSector;

u8 Fs_CdErrorCount;

u16 D5B498_8006EA1A;

StreamSlot Fs_Streams[0xa];

u16 D5B498_8006EBB0;

#include "main/fs.h"

/// What `BreakDraw` returns when it cannot suspend the DMA transfer: the SDK's
/// status value in place of a primitive address, never dereferenced.
#define FS_DRAW_BREAK_FAILED ((u_long*)-1)

#include "fs.h"
#include "main/stream.h"

/// Unreferenced.
static s32 D_8005EBC0;

/* ISO directory name suffixes / special files (must sit in .rodata before ScanIso jtbl). */
static const char Fs_ExtensionCdf[];

static const char Fs_ExtensionStr[];

static const char Fs_Stage0HeaderName[];

static const char Fs_InitBitstreamName[];

static void _gameFlowResetLoadScreenState(void);

static void _fsChunkReadyCallback(u8 interruptStatus, u8* unusedResult);

static u8 _fsReadChunkHeaderSector(void);

static u8 _fsReadChunkPayloadSector(void);

static inline void _fsStartPayloadRead(s32 startSector, s32 endSector, void* destination, u8 payloadPhase);

static void _fsStage0HeaderReadyCallback(u8 interruptStatus, u8* unusedResult);

static void Fs_ReadSector(s32 sector);

static void _fsSeekToSector(s32 absoluteSector);

static void _fsReadNSyncCallback(u8 interruptStatus, u8* unusedResult);

static void _fsPayloadReadStartedCallback(u8 interruptStatus, u8* unusedResult);

static void _fsSeekSyncCallback(u8 interruptStatus, u8* unusedResult);

static void _fsHandleCdError(u8 recoveryMode);

static void _fsResumeDrawing(u_long* resumeAddress);

/// Searches the mounted folder table from an initialized index to its active end.
///
/// Both arguments must be plain, stable local variables: they are read repeatedly
/// and index is incremented. A missing ID leaves index at the active count.
#define FILE_SYSTEM_FIND_FOLDER_INDEX(requestedId, index)                 \
    for (; (u16)(index) < Fs_FolderTableLen; (index)++) {                 \
        if ((requestedId) == Fs_FolderTable[(index) & 0xFFFF].folderId) { \
            break;                                                        \
        }                                                                 \
    }

/// Unreferenced.
static s32 D_8005EBC0 = 0;

/// Sets the VRAM rectangle and LZSS cursors for one complete image chunk.
///
/// `imageChunk` must remain readable through decoding, with compressed bytes
/// immediately after its header. Clears the selected image-status slot (0..11).
/// Source rows 245..255 receive the signed Y shift; mode 2 adds one row for
/// every image. X and dimensions are unchanged. The output cursor borrows the
/// resident decode buffer, which must fit the decoded payload. This neither
/// decodes bytes nor starts a GPU upload.
static inline void _fsPrepareImageChunkUpload(const FsImageChunk* imageChunk)
{
    enum {
        FILE_SYSTEM_IMAGE_CHUNK_SHIFT_FIRST_ROW = 245,
        FILE_SYSTEM_IMAGE_CHUNK_SHIFT_ROW_COUNT = 11,
        FILE_SYSTEM_IMAGE_CHUNK_RELOCATION_MODE = 2,
    };
    s8                  yShiftRows;
    const FsImageChunk* rectangleHeader;
    u32                 applyHeaderYShift;

    D_8006C4C8[D5B498_8006ADF4] = 0;
    Fs_ImageRect.x              = imageChunk->x;
    applyHeaderYShift           = (u32)(imageChunk->y - FILE_SYSTEM_IMAGE_CHUNK_SHIFT_FIRST_ROW) < FILE_SYSTEM_IMAGE_CHUNK_SHIFT_ROW_COUNT;
    rectangleHeader             = imageChunk;
    yShiftRows                  = applyHeaderYShift ? D5B498_8006C234 : 0;
    if (Fs_ChunkMode == FILE_SYSTEM_IMAGE_CHUNK_RELOCATION_MODE) {
        Fs_ImageRect.y = yShiftRows + (rectangleHeader->y + 1);
    } else {
        Fs_ImageRect.y = rectangleHeader->y + yShiftRows;
    }
    Fs_ChunkReadPtr  = (u8*)(imageChunk + 1);
    Fs_ImageRect.w   = rectangleHeader->w;
    Fs_ChunkWritePtr = (u8*)D5B498_8006D870;
    Fs_ImageRect.h   = rectangleHeader->h;
}

/// Clears the loading-screen phase and active latch without releasing buffers.
static void _gameFlowResetLoadScreenState(void)
{
    Fs_BootLoadPhase           = 0;
    gCdCmdQueue.bootLoadActive = false;
}

void gameFlowBeginLoadScreen(const GameLocationKey* destination, s16 alternateCaption)
{
    gCdCmdQueue.bootLoadActive = true;
    Fs_LoadParams.stage        = destination->stage;
    Fs_LoadParams.area         = destination->area;
    D5B498_8006ACC0            = alternateCaption;

    memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
    displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
    Fs_BootLoadPhase = 0;
}

void Fs_EnsureBootLoadStarted(void)
{
    if (Fs_BootLoadPhase == 0) {
        Fs_SetupBootLoad();
        Fs_BootLoadPhase = 1;
    }
}

void Fs_StepBootImage(void)
{
    switch (Fs_BootLoadPhase) {
        case 0:
            break;
        case 1:
            if (cdCmdIsSlotEmpty(Fs_BootLoadSlot)) {
                Fs_BootLoadPhase = 2;
                D5B498_8006AC9C  = 0;
            }
            break;
        case 2:
            Fs_BootImageMachine(Fs_BootTimPrimary, Fs_BootTimSecondary);
            break;
    }
}

s32 Fs_LoadFile(u8* req, s32 mode, s32 a2, s32 a3)
{
    u8  modeU8;
    s32 sector;
    u32 i;
    u16 len;
    u32 fileId;

    sector                      = 0;
    D5B498_8006ADF4             = 0;
    gCdCmdQueue.imageLoadStatus = CD_COMMAND_IMAGE_COMPLETE;
    modeU8                      = (u8)mode;

    if (req[3] == 0) {
        switch (req[2]) {
            case 0:
                if (req[1] != 0) {
                    break;
                }
                if (req[0] == 0) {
                    gDisplayState.videoMode = DISPLAY_VIDEO_STREAMING;
                }
                if (req[0] == 1) {
                    gDisplayState.videoMode = DISPLAY_VIDEO_NORMAL;
                }
                sector = Fs_FileOffsetsCat0[req[0]] + Fs_StageCdfSectors[0];
                break;

            case 1:
                fileId = (req[1] * 100) + req[0];
                len    = Fs_FileTableCat1Len;
                for (i = 0; i < (u32)len; i++) {
                    if (Fs_FileTableCat1[i].idInCategory == fileId) {
                        sector = Fs_FileTableCat1[i].sectorOffset + Fs_StageCdfSectors[0];
                        break;
                    }
                }
                break;

            case 2:
                fileId = (req[1] * 100) + req[0];
                len    = Fs_FileTableCat2Len;
                for (i = 0; i < (u32)len; i++) {
                    if (Fs_FileTableCat2[i].idInCategory == fileId) {
                        sector = Fs_FileTableCat2[i].sectorOffset + Fs_StageCdfSectors[0];
                        break;
                    }
                }
                break;

            case 3:
                fileId = (req[1] * 100) + req[0];
                len    = Fs_FileTableCat3Len;
                for (i = 0; i < (u32)len; i++) {
                    if (Fs_FileTableCat3[i].idInCategory == fileId) {
                        sector = Fs_FileTableCat3[i].sectorOffset + Fs_StageCdfSectors[0];
                        break;
                    }
                }
                break;

            case 4:
                Fs_CdOpStatus = 0xFF;
                if (req[1] == 1) {
                    fileId = req[1] * 100 + req[0];
                    len    = Fs_FileTableCat4Len;
                    for (i = 0; i < (u32)len; i++) {
                        if (Fs_FileTableCat4[i].idInCategory == fileId) {
                            sector = Fs_FileTableCat4[i].sectorOffset + Fs_StageCdfSectors[0];
                            break;
                        }
                    }
                    if (sector == 0) {
                        return 0;
                    }
                    D5B498_8006ACC8 = 1;
                    Fs_ChunkMode    = 0;
                    Fs_ReadSector(sector);
                }
                return sector & 0xFFFF;

            case 5:
                sector = Fs_FileOffsetsCat5[req[0]] + Fs_StageCdfSectors[0];
                break;

            case 0x5A:
                sector = Fs_FileOffsetsCat90[req[0]] + Fs_StageCdfSectors[0];
                break;

            default:
                D5B498_8006ADF4 = req[2] / 10;
                fileId          = (req[2] * 10000) + (req[1] * 100) + req[0];
                if (D5B498_8006ADF4 != 0) {
                    len = Fs_FileTableLen;
                    for (i = 0; i < (u32)len; i++) {
                        if (Fs_FileTable[i].fileId == fileId) {
                            sector = Fs_FileTable[i].sectorOffset + Fs_StageCdfSectors[0];
                            if (req[2] == 8) {
                                if ((u32)(req[1] - 1) < 3U) {
                                    gGameSession->field_4E = 1;
                                }
                            }
                            break;
                        }
                    }
                }
                break;
        }
    } else {
        sector = D_8006C158[req[0]] + Fs_StageCdfSectors[req[3]];
    }

    D5B498_8006C234 = a3;
    D5B498_8006C233 = a2;
    D5B498_8006ACC8 = 0;
    if (sector != 0) {
        switch (modeU8) {
            case 0:
                Fs_ChunkMode = 0;
                break;
            case 1:
                _fsSeekToSector(sector);
                return sector & 0xFFFF;
            case 2:
                Snd_InitFromStage(gGameSession->location.loc.stage, gGameSession->location.loc.area);
                Fs_ChunkMode = 1;
                break;
            case 3:
                Fs_ChunkMode = 2;
                break;
            case 4:
                Fs_ChunkMode = 3;
                break;
            case 5:
                Fs_ChunkMode = 4;
                break;
            case 6:
                Fs_ChunkMode = 5;
                break;
            default:
                Fs_ChunkMode = 0;
                break;
        }
        Fs_ReadSector(sector);
    }
    return sector & 0xFFFF;
}

/// Validates a sector and feeds the active CDF header or payload reader.
///
/// Installed only for serialized sector-header reads. Result bytes are unused;
/// every interrupt except CdlDiskError takes the data path. A payload-position
/// mismatch resumes the chunk, except music, which restarts the request.
/// A completed load pauses the drive, detaches this callback and resets image
/// load state before publishing the idle marker.
static void _fsChunkReadyCallback(u8 interruptStatus, u8* unusedResult)
{
    struct {
        CdlLOC location;       // BCD minute, second and sector
        u32    unreadWords[2]; // Remaining header bytes; not interpreted here
    } sectorHeader;
    s32 sectorPosition;
    u8  loadComplete;

    if (interruptStatus != CdlDiskError) {
        Fs_VBlank = VSync(-1);
        CdGetSector(&sectorHeader, sizeof(sectorHeader) / sizeof(u32));
        Fs_CurrSector = sectorPosition = CdPosToInt(&sectorHeader.location);

        if (sectorPosition != Fs_ReqSector) {
            if ((Fs_Streaming != 0) && (Fs_LoadPhase != FILE_SYSTEM_PAYLOAD_MUSIC)) {
                _fsHandleCdError(FS_ERROR_HARD);
                return;
            }
            _fsHandleCdError(FS_ERROR_SOFT);
            return;
        }

        Fs_ReqSector = sectorPosition + 1;
        if (Fs_Streaming == 0) {
            loadComplete = _fsReadChunkHeaderSector();
        } else {
            loadComplete = _fsReadChunkPayloadSector();
        }
    } else {
        _fsHandleCdError(FS_ERROR_SOFT);
        return;
    }

    if (loadComplete != 0) {
        CdControlF(CdlPause, NULL);
        CdReadyCallback(NULL);
        D5B498_8006C234 = 0;
        D5B498_8006C233 = 0;
        D5B498_8006ADF4 = 0;
        Fs_ChunkMode    = FILE_SYSTEM_CHUNK_LOAD_NORMAL;
        Fs_CdOpStatus   = FS_CD_STATUS_IDLE;
    }
}

/// Reads a CDF chunk's opening sector and prepares its payload interpretation.
///
/// `Fs_ReqSector` is already the next sector. Returns 1 to stop the whole load,
/// otherwise 0. Header sector counts/valid-byte ends and RAM destinations must
/// be valid for the opcode; raw destinations are word aligned. Bundle directories
/// contain all fifty resource slots. Image tables/streams must satisfy their
/// upload contracts and background bytes must fit the image workspace.
/// CLUT chunks have one or two sectors; a continuation is consumed in one call.
static u8 _fsReadChunkHeaderSector(void)
{
    s32                     entryIndex = 0;
    const FsCdfChunkHeader* chunkHeader;
    s32                     payloadStatus;

    // Set the exclusive sector end and the per-sector compressed-input boundary.
    CdGetSector(&Fs_CdSector, FS_SECTOR_WORD_SIZE);
    D_8006C4D4        = Fs_CdSector.bytes;
    chunkHeader       = &Fs_CdSector.chunk.header;
    Fs_ChunkWritePtr  = chunkHeader->loadAddr;
    Fs_ChunkEndSector = Fs_ReqSector - 1 + chunkHeader->sectorCount;
    Fs_ChunkEndFlag   = chunkHeader->endFlag;
    D_8006C4D4       += chunkHeader->sectorLen;

    switch (Fs_CdSector.chunk.header.type) {
        case FILE_SYSTEM_CHUNK_PACKAGE:
            if (Fs_ChunkWritePtr == NULL) {
                break;
            }
            if (Fs_ChunkMode == FILE_SYSTEM_CHUNK_LOAD_INIT_SOUND || Fs_ChunkMode == FILE_SYSTEM_CHUNK_LOAD_IMAGES_ONLY) {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                return 1;
            }
            D5B498_8006EA1A = 0;
            D5B498_8006EBB0 = 0;
            D5B498_8006D858 = 1;
            D5B498_8006D850 = 0;
            D5B498_8006D748 = FILE_SYSTEM_STREAM_DECODE_NEEDS_INPUT;
            Fs_ChunkReadPtr = Fs_CdSector.chunk.data.bytes;
            fsDecompressStream();
            if (D5B498_8006D748 == FILE_SYSTEM_STREAM_DECODE_SCRATCH_BUSY) {
                _fsHandleCdError(FS_ERROR_SOFT);
                break;
            }
            if (D5B498_8006D748 != FILE_SYSTEM_STREAM_DECODE_NEEDS_INPUT) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                    return 1;
                }
                break;
            }
            Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_PACKAGE;
            Fs_Streaming = 1;
            break;

        case FILE_SYSTEM_CHUNK_IMAGE:
            fsBeginImageColumns((FsImageColumn*)Fs_CdSector.chunk.data.bytes);
            payloadStatus = fsUploadImageStrips(FILE_SYSTEM_IMAGE_STRIPS_CD_INPUT);
            if (payloadStatus == FILE_SYSTEM_IMAGE_STRIPS_TIMER_FAILED || payloadStatus == FILE_SYSTEM_IMAGE_STRIPS_RETRY) {
                _fsHandleCdError(FS_ERROR_SOFT);
                break;
            }
            if (payloadStatus == FILE_SYSTEM_IMAGE_STRIPS_COMPLETE) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                    return 1;
                }
            } else {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_IMAGE;
                Fs_Streaming = 1;
            }
            break;

        case FILE_SYSTEM_CHUNK_CLUT:
            if (Fs_ChunkEndSector == Fs_ReqSector) {
                payloadStatus = fsUploadImageChunk((const FsImageChunk*)Fs_CdSector.chunk.data.bytes, 0);
                if (payloadStatus == FILE_SYSTEM_IMAGE_UPLOAD_TIMER_FAILED || payloadStatus == FILE_SYSTEM_IMAGE_UPLOAD_RETRY) {
                    _fsHandleCdError(FS_ERROR_SOFT);
                    break;
                }
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                    return 1;
                }
            } else {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_CLUT;
                Fs_Streaming = 1;
            }
            break;

        case FILE_SYSTEM_CHUNK_RAW: {
            u32* sourceWords;
            u32* destinationWords;
            if (Fs_ChunkMode == FILE_SYSTEM_CHUNK_LOAD_INIT_SOUND || Fs_ChunkMode == FILE_SYSTEM_CHUNK_LOAD_IMAGES_ONLY) {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                return 1;
            }
            sourceWords      = (u32*)&Fs_CdSector.chunk.data.bytes[entryIndex];
            destinationWords = (u32*)Fs_ChunkWritePtr;
            for (entryIndex = 0; entryIndex < ARRAY_SIZE(Fs_CdSector.chunk.data.words); entryIndex++) {
                destinationWords[entryIndex] = sourceWords[entryIndex];
            }
            Fs_ChunkWritePtr += sizeof(Fs_CdSector.chunk.data.bytes);
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                    return 1;
                }
            } else {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_RAW;
                Fs_Streaming = 1;
            }
            break;
        }

        case FILE_SYSTEM_CHUNK_BUNDLE: {
            _FsCdfResourceEntry* resourceEntry;
            if (Fs_ChunkMode == FILE_SYSTEM_CHUNK_LOAD_INIT_SOUND || Fs_ChunkMode == FILE_SYSTEM_CHUNK_LOAD_IMAGES_ONLY) {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                return 1;
            }
            resourceEntry = (_FsCdfResourceEntry*)Fs_CdSector.chunk.data.bytes;
            // Publish resource destinations before streaming the bundle's payload.
            for (entryIndex = 0; entryIndex < ARRAY_SIZE(D_8006C338); entryIndex++) {
                D_8006C338[entryIndex].kind = resourceEntry->kind;
                D_8006C338[entryIndex].data = resourceEntry->destination;
                if (resourceEntry->redirectDestination != NULL) {
                    Fs_LoadRedirect.enabled               = 1;
                    Fs_LoadRedirect.sectorsBeforeRedirect = resourceEntry->sectorsBeforeRedirect;
                    Fs_LoadRedirect.destination           = resourceEntry->redirectDestination;
                }
                resourceEntry++;
            }
            Fs_LoadRedirect.sectorsRead = 0;
            Fs_LoadPhase                = FILE_SYSTEM_PAYLOAD_BUNDLE;
            Fs_Streaming                = 1;
            break;
        }

        case FILE_SYSTEM_CHUNK_BACKGROUND: {
            u8* backgroundBytes;
            if (Fs_ChunkMode != FILE_SYSTEM_CHUNK_LOAD_SKIP_BACKGROUND) {
                mdecRequestImageVlcRebuild();
                backgroundBytes = (u8*)Fs_ImgBuffers;
                for (D_8006ADF8 = 0; (u32)D_8006ADF8 < sizeof(Fs_CdSector.chunk.data.bytes); D_8006ADF8++) {
                    backgroundBytes[D_8006ADF8] = Fs_CdSector.chunk.data.bytes[D_8006ADF8];
                }
            }
            Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_BACKGROUND;
            Fs_Streaming = 1;
            break;
        }

        case FILE_SYSTEM_CHUNK_MUSIC:
            if (Fs_ChunkMode == FILE_SYSTEM_CHUNK_LOAD_IMAGES_ONLY || Fs_ChunkMode == FILE_SYSTEM_CHUNK_LOAD_SKIP_SOUND) {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                Fs_Streaming = 1;
                break;
            }
            sndLoadBeginChunkLoad(0, Fs_CdSector.bytes);
            payloadStatus = sndLoadFeedChunkSector(Fs_CdSector.bytes);
            if (payloadStatus == SOUND_LOAD_PHASE_DONE) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                    return 1;
                }
            } else if (payloadStatus == SOUND_LOAD_RESULT_ERROR) {
                _fsHandleCdError(FS_ERROR_SOFT);
                break;
            } else {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_MUSIC;
                Fs_Streaming = 1;
            }
            break;

        case FILE_SYSTEM_CHUNK_TEXT:
            if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                return 1;
            }
            break;

        default:
            if (Fs_ChunkEndSector == Fs_ReqSector) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                    return 1;
                }
            } else {
                Fs_LoadPhase = FILE_SYSTEM_PAYLOAD_SKIP;
                Fs_Streaming = 1;
            }
            break;
    }
    return 0;
}

/// Feeds one continuation sector to the selected payload reader.
///
/// `Fs_ReqSector` already names the next sector, and `Fs_ChunkEndSector` is the
/// exclusive chunk end. Raw/bundle destinations need a whole writable sector;
/// package output and image/background buffers must fit the decoded/copied data.
/// Returns 1 when the load ends; otherwise switches back to headers at chunk
/// completion. Timer-reset failure repeats this sector; contention restarts the
/// chunk request. A CLUT continuation depends on the adjacent sector buffers.
static u8 _fsReadChunkPayloadSector(void)
{
    // Values of the selected load slot minus one, and its output-size policy.
    enum {
        FILE_SYSTEM_PACKAGE_OUTPUT_BASE0                   = 0,
        FILE_SYSTEM_PACKAGE_OUTPUT_BASE1                   = 1,
        FILE_SYSTEM_PACKAGE_OUTPUT_BASE2                   = 2,
        FILE_SYSTEM_PACKAGE_OUTPUT_BASE0_INVALIDATE_BASE1  = 3,
        FILE_SYSTEM_PACKAGE_OUTPUT_BASE0_INVALIDATE_OTHERS = 4,
        FILE_SYSTEM_PACKAGE_OUTPUT_BASE2_ALTERNATE         = 7,
        FILE_SYSTEM_PACKAGE_OUTPUT_PENDING_BYTES           = -1,
    };
    s32  payloadStatus;
    s32  endMarker;
    s32* outputSizes;

    switch (Fs_LoadPhase) {
        case FILE_SYSTEM_PAYLOAD_RAW:
            CdGetSector(Fs_ChunkWritePtr, FS_SECTOR_WORD_SIZE);
            Fs_ChunkWritePtr += FS_SECTOR_BYTE_SIZE;
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                endMarker = Fs_ChunkEndFlag;
                if (endMarker == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endMarker;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case FILE_SYSTEM_PAYLOAD_PACKAGE:
            CdGetSector(Fs_CdSector.bytes, FS_SECTOR_WORD_SIZE);
            Fs_ChunkReadPtr = Fs_CdSector.bytes;
            fsDecompressStream();
            if (D5B498_8006D748 == FILE_SYSTEM_STREAM_DECODE_SCRATCH_BUSY) {
                _fsHandleCdError(FS_ERROR_SOFT);
            } else if (D5B498_8006D748 != FILE_SYSTEM_STREAM_DECODE_NEEDS_INPUT || (u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                switch (D5B498_8006ADF4 - 1) {
                    case FILE_SYSTEM_PACKAGE_OUTPUT_BASE0:
                        Fs_ChunkOutputSizes[0] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase0;
                        break;
                    case FILE_SYSTEM_PACKAGE_OUTPUT_BASE1:
                        Fs_ChunkOutputSizes[1] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase1;
                        break;
                    case FILE_SYSTEM_PACKAGE_OUTPUT_BASE2:
                    case FILE_SYSTEM_PACKAGE_OUTPUT_BASE2_ALTERNATE:
                        Fs_ChunkOutputSizes[2] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase2;
                        break;
                    case FILE_SYSTEM_PACKAGE_OUTPUT_BASE0_INVALIDATE_BASE1:
                        outputSizes            = Fs_ChunkOutputSizes;
                        outputSizes[1]         = FILE_SYSTEM_PACKAGE_OUTPUT_PENDING_BYTES;
                        Fs_ChunkOutputSizes[0] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase0;
                        break;
                    case FILE_SYSTEM_PACKAGE_OUTPUT_BASE0_INVALIDATE_OTHERS: {
                        s32* pendingOutputSizes;
                        pendingOutputSizes     = Fs_ChunkOutputSizes;
                        pendingOutputSizes[1]  = FILE_SYSTEM_PACKAGE_OUTPUT_PENDING_BYTES;
                        pendingOutputSizes[2]  = FILE_SYSTEM_PACKAGE_OUTPUT_PENDING_BYTES;
                        Fs_ChunkOutputSizes[0] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase0;
                        break;
                    }
                }
                endMarker = Fs_ChunkEndFlag;
                if (endMarker == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endMarker;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case FILE_SYSTEM_PAYLOAD_IMAGE:
            CdGetSector(Fs_CdSector.bytes, FS_SECTOR_WORD_SIZE);
            payloadStatus = fsUploadImageStrips(FILE_SYSTEM_IMAGE_STRIPS_CD_INPUT);
            if (payloadStatus == FILE_SYSTEM_IMAGE_STRIPS_TIMER_FAILED) {
                Fs_ReqSector--;
                _fsHandleCdError(FS_ERROR_HARD);
            } else if (payloadStatus == FILE_SYSTEM_IMAGE_STRIPS_RETRY) {
                _fsHandleCdError(FS_ERROR_SOFT);
            } else if (payloadStatus == FILE_SYSTEM_IMAGE_STRIPS_COMPLETE || (u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                endMarker = Fs_ChunkEndFlag;
                if (endMarker == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endMarker;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case FILE_SYSTEM_PAYLOAD_CLUT:
            CdGetSector(D_8006CCD8, FS_SECTOR_WORD_SIZE);
            // The image header is the start of the previous sector's payload, contiguous with this continuation sector.
            payloadStatus = fsUploadImageChunk((const FsImageChunk*)(D_8006CCD8 - sizeof(Fs_CdSector.chunk.data.bytes)), 0);
            if (payloadStatus == FILE_SYSTEM_IMAGE_UPLOAD_TIMER_FAILED) {
                Fs_ReqSector--;
                _fsHandleCdError(FS_ERROR_HARD);
            } else if (payloadStatus == FILE_SYSTEM_IMAGE_UPLOAD_RETRY) {
                _fsHandleCdError(FS_ERROR_SOFT);
            } else {
                endMarker = Fs_ChunkEndFlag;
                if (endMarker == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endMarker;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case FILE_SYSTEM_PAYLOAD_BUNDLE:
            CdGetSector(Fs_ChunkWritePtr, FS_SECTOR_WORD_SIZE);
            Fs_ChunkWritePtr += FS_SECTOR_BYTE_SIZE;
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                endMarker = Fs_ChunkEndFlag;
                if (endMarker == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endMarker;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            if (Fs_LoadRedirect.enabled != 0) {
                Fs_LoadRedirect.sectorsRead++;
                if (Fs_LoadRedirect.sectorsBeforeRedirect == Fs_LoadRedirect.sectorsRead) {
                    Fs_ChunkWritePtr = Fs_LoadRedirect.destination;
                }
            }
            break;
        case FILE_SYSTEM_PAYLOAD_BACKGROUND:
            if (Fs_ChunkMode != FILE_SYSTEM_CHUNK_LOAD_SKIP_BACKGROUND) {
                CdGetSector((u8*)Fs_ImgBuffers->strips + D_8006ADF8, FS_SECTOR_WORD_SIZE);
                D_8006ADF8 += FS_SECTOR_BYTE_SIZE;
            }
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                if (Fs_ChunkMode != FILE_SYSTEM_CHUNK_LOAD_SKIP_BACKGROUND) {
                    mdecRequestImageDecode(Fs_ImgBuffers->strips[0]);
                }
                endMarker = Fs_ChunkEndFlag;
                if (endMarker == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endMarker;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case FILE_SYSTEM_PAYLOAD_MUSIC:
            CdGetSector(Fs_CdSector.bytes, FS_SECTOR_WORD_SIZE);
            payloadStatus = sndLoadFeedChunkSector(Fs_CdSector.bytes);
            if (payloadStatus == SOUND_LOAD_PHASE_DONE) {
                endMarker = Fs_ChunkEndFlag;
                if (endMarker == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endMarker;
                    return 1;
                }
                Fs_Streaming = 0;
            } else if (payloadStatus == SOUND_LOAD_RESULT_ERROR) {
                _fsHandleCdError(FS_ERROR_SOFT);
            }
            break;
        default:
            if (Fs_ChunkEndSector == Fs_ReqSector) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
    }
    return 0;
}

/// Starts serialized sector-header reads with preconfigured payload processing.
///
/// Absolute sectors, exclusive end and destination lifetime follow
/// `fsStartPayloadRead`. Does not reset chunk-load policy or the selected slot.
/// A pending drive error is recovered before publishing the transfer cursors;
/// a seek at the start sector is reused. The first callback enables payload
/// handling without consuming a header or sector itself.
static inline void _fsStartPayloadRead(s32 startSector, s32 endSector, void* destination, u8 payloadPhase)
{
    CdlLOC readLocation;

    if (CdSync(1, NULL) == CdlDiskError) {
        cdSyncWaitForReadableDisc(true);
    }

    Fs_CdOpStatus     = FILE_SYSTEM_CD_OPERATION_PENDING;
    Fs_ChunkEndFlag   = FILE_SYSTEM_CHUNK_LAST;
    Fs_LoadPhase      = payloadPhase;
    Fs_ReqSector      = startSector;
    Fs_ChunkEndSector = endSector;
    Fs_ChunkWritePtr  = destination;
    Fs_VBlank         = VSync(-1);
    if (Fs_SeekSector == startSector) {
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(_fsPayloadReadStartedCallback);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(startSector, &readLocation);
        CdControlF(CdlReadN, &readLocation.minute);
        CdSyncCallback(_fsPayloadReadStartedCallback);
        Fs_SeekSector = 0;
    }
}

void fsStartStageFolderListRead(s32 stageIndex)
{
    s32 stageSector;
    u8* destination;

    Fs_ChunkMode    = FILE_SYSTEM_CHUNK_LOAD_NORMAL;
    D5B498_8006ADF4 = 0;
    stageSector     = Fs_StageCdfSectors[(u8)stageIndex];
    destination     = Fs_CdSector.bytes;

    _fsStartPayloadRead(stageSector, stageSector, destination, FILE_SYSTEM_PAYLOAD_RAW);
    Fs_VBlank = VSync(-1);
}

void fsStartFolderDirectoryRead(s32 stageIndex, s32 fileGroup, s32 folderIndex)
{
    u16 slotIndex;
    s32 folderId;
    s32 directorySector;

    // Retire published slots and pending redirects before loading another directory.
    Fs_LoadRedirect.enabled               = 0;
    Fs_LoadRedirect.sectorsRead           = 0;
    Fs_LoadRedirect.sectorsBeforeRedirect = 0;
    Fs_LoadRedirect.destination           = NULL;
    Fs_ChunkMode                          = FILE_SYSTEM_CHUNK_LOAD_NORMAL;
    D5B498_8006ADF4                       = 0;

    for (slotIndex = 0; slotIndex < ARRAY_SIZE(D_8006C338); slotIndex++) {
        D_8006C338[slotIndex].kind = FILE_SYSTEM_RESOURCE_NONE;
    }

    for (slotIndex = 0; slotIndex < ARRAY_SIZE(Stream_Slots); slotIndex++) {
        Stream_Slots[slotIndex].startSector = 0;
    }

    D_8006ADE2 = 0;
    folderId   = ((u8)fileGroup * 100) + (u8)folderIndex;

    slotIndex = 0;
    FILE_SYSTEM_FIND_FOLDER_INDEX(folderId, slotIndex);

    Fs_VBlank = VSync(-1);

    directorySector = Fs_FolderTable[slotIndex].sectorOffset + Fs_StageCdfSectors[(u8)stageIndex];
    _fsStartPayloadRead(directorySector, directorySector, Fs_CdSector.bytes, FILE_SYSTEM_PAYLOAD_RAW);
}

/// Copies all bytes of one stream descriptor into a distinct writable slot.
static inline void _fsCopyStreamSlot(StreamSlot* destination, const StreamSlot* source)
{
    s32       byteIndex;
    const u8* sourceBytes;
    u8*       destinationBytes;

    sourceBytes      = (const u8*)source;
    destinationBytes = (u8*)destination;
    for (byteIndex = 0; (u16)byteIndex < sizeof(*destination); byteIndex++) {
        destinationBytes[byteIndex & 0xFFFF] = sourceBytes[byteIndex & 0xFFFF];
    }
}

void fsBuildFolderTables(s32 stage, s32 fileGroup, s32 folderIndex)
{
    enum { FILE_SYSTEM_FOLDER_STREAM_TABLE_OFFSET = 0x514 };
    STATIC_ASSERT(sizeof(Fs_CdSector.fileList) <= FILE_SYSTEM_FOLDER_STREAM_TABLE_OFFSET, fs_file_list_precedes_stream_table);
    s32                 folderTableIndex;
    s32                 entryIndex;
    s32                 folderId;
    const FsCdfFile*    fileList;
    const _FsCdfFolder* streamFolder;
    s32*                fileSectors;
    const s32*          stageBaseSector;
    s32                 fileSectorOffset;
    const _FsCdfFolder* fileFolder;
    StreamSlot*         sourceStreams;
    StreamSlot*         destinationStreams;

    folderTableIndex = 0;
    folderId         = ((u8)fileGroup * 100) + (u8)folderIndex;
    FILE_SYSTEM_FIND_FOLDER_INDEX(folderId, folderTableIndex);

    fileList    = Fs_CdSector.fileList;
    entryIndex  = 0;
    fileSectors = D_8006C158;
    {
        _FsCdfFolder* folderTable = Fs_FolderTable;
        fileFolder                = folderTable + (folderTableIndex & 0xFFFF);
    }
    for (;;) {
        fileSectorOffset = fileList[entryIndex & 0xFFFF].sectorOffset;
        if (fileSectorOffset != 0) {
            fileSectors[fileList[entryIndex & 0xFFFF].fileId] = fileSectorOffset + fileFolder->sectorOffset;
            entryIndex                                       += 1;
        } else {
            break;
        }
    }

    // Streams use the area's room folder even when files came from another folder.
    folderTableIndex = 0;
    folderId         = ((u8)fileGroup * 100) + 1;
    sourceStreams    = (StreamSlot*)(Fs_CdSector.bytes + FILE_SYSTEM_FOLDER_STREAM_TABLE_OFFSET);
    FILE_SYSTEM_FIND_FOLDER_INDEX(folderId, folderTableIndex);

    entryIndex = 0;
    {
        _FsCdfFolder* folderTable = Fs_FolderTable;
        streamFolder              = folderTable + (folderTableIndex & 0xFFFF);
    }
    {
        const s32* stageSectors = Fs_StageCdfSectors;
        stageBaseSector         = stageSectors + (u8)stage;
    }
    // Publish complete descriptors with absolute CD sectors.
    destinationStreams = Stream_Slots;
    for (;;) {
        if (sourceStreams[entryIndex & 0xFFFF].key.word != STREAM_KEY_TERMINATOR) {
            sourceStreams[entryIndex & 0xFFFF].startSector += streamFolder->sectorOffset + *stageBaseSector;
            _fsCopyStreamSlot(&destinationStreams[entryIndex & 0xFFFF], &sourceStreams[entryIndex & 0xFFFF]);
            entryIndex += 1;
        } else {
            break;
        }
    }
}

#undef FILE_SYSTEM_FIND_FOLDER_INDEX

/// Imports one expected HED sector into the STAGE0 file and stream tables.
///
/// Runs with sector headers enabled; result bytes are unused. The -1 word ends
/// the HED, while other high-bit words mark complete `StreamSlot` records. File
/// records are `FsCdfFile` pairs. Each record must fit wholly within this sector;
/// category 0 requires 45 consecutive file pairs in the same sector. Appended
/// records must fit their tables, including at most ten streams in each sector.
/// The stream index restarts per callback, so a later sector replaces slots.
/// Category 5/90 IDs modulo 100 must be valid indices. Category 1-4 retain
/// category-local IDs (0..9999) and truncate sector offsets to 16 bits, so input
/// offsets must be representable. These are unchecked input preconditions.
static void _fsStage0HeaderReadyCallback(u8 interruptStatus, u8* unusedResult)
{
    enum {
        FILE_SYSTEM_HED_STREAM_HEADER_MASK   = 0x7FFFFFFF,
        FILE_SYSTEM_HED_CATEGORY0_FILE_COUNT = 45,
        FILE_SYSTEM_HED_FILE_CATEGORY_SCALE  = 10000,
        FILE_SYSTEM_HED_FILE_INDEX_SCALE     = 100,
        FILE_SYSTEM_HED_FULL_FILE_ID_MIN     = 100000,
    };
    struct {
        CdlLOC location;                        // BCD position at the start of the sector header
        u8     remainingBytes[2 * sizeof(u32)]; // Uninterpreted remainder of the 12-byte header
    } sectorHeader;
    s32                    absoluteSector;
    u32                    recordWordOffset;
    u32                    sectorStreamIndex;
    u32                    fileId;
    u32                    fileCategory;
    u8                     categoryHandled;
    FsCdfFile*             fileRecord;
    _FsStage0CategoryFile* categoryTable;
    FsSector*              sectorBuffer;
    FsCdfFile*             fileCursor;
    StreamSlot*            streamTable;
    u32*                   category90Sectors;
    u16*                   category5Sectors;
    u16*                   category0Sectors;

    sectorStreamIndex = 0;
    if (interruptStatus != CdlDiskError) {
        // Validate the disc position before consuming the sector payload.
        Fs_VBlank = VSync(-1);
        CdGetSector(&sectorHeader, sizeof(sectorHeader) / sizeof(u32));

        Fs_CurrSector = absoluteSector = CdPosToInt(&sectorHeader.location);
        if (absoluteSector != Fs_ReqSector) {
            _fsHandleCdError(FS_ERROR_SOFT);
            return;
        }

        Fs_ReqSector += 1;
        CdGetSector(Fs_CdSector.words, FS_SECTOR_WORD_SIZE);

        recordWordOffset = 0;

        while (true) {
            sectorBuffer      = &Fs_CdSector;
            streamTable       = Fs_Streams;
            category0Sectors  = Fs_FileOffsetsCat0;
            category5Sectors  = Fs_FileOffsetsCat5;
            category90Sectors = Fs_FileOffsetsCat90;

            if ((u16)recordWordOffset >= FS_SECTOR_WORD_SIZE) {
                return;
            }

            fileRecord = (FsCdfFile*)&sectorBuffer->words[(u16)recordWordOffset];
            fileId     = fileRecord->fileId;
            if (fileId == FS_CDF_STAGE0_CANARY) {
                CdReadyCallback(NULL);
                Fs_CdOpStatus = FS_CD_STATUS_IDLE;
                CdControlF(CdlPause, NULL);
                return;
            }

            if ((s32)fileId < 0) {
                // Remove the serialized stream marker and publish an absolute sector.
                fileRecord->fileId &= FILE_SYSTEM_HED_STREAM_HEADER_MASK;
                _fsCopyStreamSlot(&streamTable[(u16)sectorStreamIndex], (const StreamSlot*)fileRecord);
                streamTable[(u16)sectorStreamIndex++].startSector += Fs_StageCdfSectors[0];
                recordWordOffset                                  += sizeof(StreamSlot) / sizeof(u32);
            } else {
                fileCategory = fileId / FILE_SYSTEM_HED_FILE_CATEGORY_SCALE;

                categoryHandled = false;
                switch (fileCategory) {
                    case 0: {
                        u32 category0Index = 0;

                        // This indexed run has a fixed serialized length, not the table capacity.
                        while (true) {
                            category0Sectors[(u16)category0Index] = ((FsCdfFile*)&sectorBuffer->words[(u16)recordWordOffset])->sectorOffset;
                            category0Index++;
                            if ((u16)category0Index >= FILE_SYSTEM_HED_CATEGORY0_FILE_COUNT) {
                                break;
                            }
                            recordWordOffset += sizeof(FsCdfFile) / sizeof(u32);
                        }
                        categoryHandled = true;
                        break;
                    }

                    case 1: {
                        u32 tableIndex;

                        categoryHandled = true;
                        categoryTable   = Fs_FileTableCat1;
                        tableIndex      = Fs_FileTableCat1Len;
                        Fs_FileTableCat1Len++;
                        categoryTable[tableIndex].idInCategory = fileId - FILE_SYSTEM_HED_FILE_CATEGORY_SCALE;
                        categoryTable[tableIndex].sectorOffset = fileRecord->sectorOffset;
                        break;
                    }

                    case 2: {
                        u32 tableIndex;

                        categoryHandled = true;
                        categoryTable   = Fs_FileTableCat2;
                        tableIndex      = Fs_FileTableCat2Len;
                        Fs_FileTableCat2Len++;
                        categoryTable[tableIndex].idInCategory = fileId - fileCategory * FILE_SYSTEM_HED_FILE_CATEGORY_SCALE;
                        categoryTable[tableIndex].sectorOffset = fileRecord->sectorOffset;
                        break;
                    }

                    case 3: {
                        u32 tableIndex;

                        categoryHandled = true;
                        categoryTable   = Fs_FileTableCat3;
                        tableIndex      = Fs_FileTableCat3Len;
                        Fs_FileTableCat3Len++;
                        categoryTable[tableIndex].idInCategory = fileId - 3 * FILE_SYSTEM_HED_FILE_CATEGORY_SCALE;
                        categoryTable[tableIndex].sectorOffset = fileRecord->sectorOffset;
                        break;
                    }

                    case 4: {
                        u32 tableIndex;

                        categoryHandled = true;
                        categoryTable   = Fs_FileTableCat4;
                        tableIndex      = Fs_FileTableCat4Len;
                        Fs_FileTableCat4Len++;
                        categoryTable[tableIndex].idInCategory = fileId - fileCategory * FILE_SYSTEM_HED_FILE_CATEGORY_SCALE;
                        categoryTable[tableIndex].sectorOffset = fileRecord->sectorOffset;
                        break;
                    }

                    case 5:
                        category5Sectors[fileRecord->fileId % FILE_SYSTEM_HED_FILE_INDEX_SCALE] = fileRecord->sectorOffset;
                        categoryHandled                                                         = true;
                        break;

                    case 90:
                        category90Sectors[fileRecord->fileId % FILE_SYSTEM_HED_FILE_INDEX_SCALE] = fileRecord->sectorOffset;
                        categoryHandled                                                          = true;
                        break;
                }

                if (!categoryHandled) {
                    u32 uncategorizedFileId;

                    // Mixed HED strides count words even when the cursor holds file pairs.
                    fileCursor          = (FsCdfFile*)sectorBuffer->words;
                    uncategorizedFileId = ((FsCdfFile*)&((u32*)fileCursor)[(u16)recordWordOffset])->fileId;
                    if (uncategorizedFileId / FILE_SYSTEM_HED_FULL_FILE_ID_MIN != 0) {
                        u32 tableIndex;

                        fileCursor = Fs_FileTable;
                        tableIndex = Fs_FileTableLen;
                        Fs_FileTableLen++;
                        fileCursor[tableIndex].fileId       = uncategorizedFileId;
                        fileCursor[tableIndex].sectorOffset = ((FsCdfFile*)&sectorBuffer->words[(u16)recordWordOffset])->sectorOffset;
                    }
                }

                recordWordOffset += sizeof(FsCdfFile) / sizeof(u32);
            }
        }
    }

    _fsHandleCdError(FS_ERROR_SOFT);
}

/* ISO directory name suffixes / special files (must sit in .rodata before ScanIso jtbl). */
static const char Fs_ExtensionCdf[]      = ".CDF";
static const char Fs_ExtensionStr[]      = ".STR";
static const char Fs_Stage0HeaderName[]  = "STAGE0.HED";
static const char Fs_InitBitstreamName[] = "INIT.BS";

/// Starts the discovered STAGE0.HED read to rebuild resident file/stream tables.
///
/// The ISO scan must have published a nonzero `Fs_Stage0HedSector` and the
/// STAGE0.CDF base sector. Requires exclusive filesystem/CD state with sector
/// headers enabled. Resets append counts without clearing table storage;
/// `_fsStage0HeaderReadyCallback` imports records until the HED terminator.
/// Returns before completion: `Fs_CdOpStatus` reports 0xFF done or 0x80 restart.
/// The final VBlank counter sample starts the read-timeout interval.
static inline void _fsStartScannedStage0HeaderRead(void)
{
    enum {
        FILE_SYSTEM_VSYNC_QUERY_COUNTER = -1,
    };
    CdlLOC headerLocation;

    // Appended tables start empty; indexed tables are overwritten by the HED.
    Fs_CdOpStatus       = FILE_SYSTEM_CD_OPERATION_PENDING;
    Fs_FileTableLen     = 0;
    Fs_FileTableCat2Len = 0;
    Fs_FileTableCat4Len = 0;
    Fs_FileTableCat1Len = 0;
    Fs_FileTableCat3Len = 0;

    // Publish the expected absolute sector before enabling its ready callback.
    Fs_ReqSector = Fs_Stage0HedSector;
    CdIntToPos(Fs_Stage0HedSector, &headerLocation);
    CdControlF(CdlReadN, &headerLocation.minute);
    CdReadyCallback(_fsStage0HeaderReadyCallback);
    Fs_VBlank = VSync(FILE_SYSTEM_VSYNC_QUERY_COUNTER);
}

void fsScanIsoDirectory(s32 bootMode)
{
    enum {
        FILE_SYSTEM_ISO_DIRECTORY_SECTOR          = 22,
        FILE_SYSTEM_ISO_FALLBACK_DIRECTORY_SECTOR = 20,
        FILE_SYSTEM_ISO_BASE_RECORD_BYTES         = 48,
        FILE_SYSTEM_ISO_NAMED_RECORD_BYTES        = 56,
        FILE_SYSTEM_ISO_EXTENT_LOW_OFFSET         = 2,
        FILE_SYSTEM_ISO_EXTENT_HIGH_OFFSET        = 4,
        FILE_SYSTEM_ISO_NAME_OFFSET               = 33,
        FILE_SYSTEM_ISO_EXTENSION_BYTES           = 4,
        FILE_SYSTEM_STARTUP_IMAGE_SECTORS         = 16,
    };
    u8* directoryRecord;
    s32 startupImageSector;
    s32 startupImageSectorCount;
    s32 directoryDone;
    s32 nameByteIndex;
    s32 hasExtension;
    u8  stageIndex;
    s32 nameByte;

    startupImageSector      = 0;
    startupImageSectorCount = 0;

    SetMem(2);

// Recovery repeats directory discovery because the disc may have changed.
restart:
    _fsStartPayloadRead(FILE_SYSTEM_ISO_DIRECTORY_SECTOR, FILE_SYSTEM_ISO_DIRECTORY_SECTOR, Fs_CdSector.bytes, FILE_SYSTEM_PAYLOAD_RAW);
    while (Fs_CdOpStatus != (u8)FS_CD_STATUS_IDLE) {
        if (Fs_CdOpStatus == FILE_SYSTEM_CD_OPERATION_RESTART_REQUEST) {
            cdSyncWaitForCommandCompletion();
            goto restart;
        }
        VSync(0);
    }

    {
        u8* directorySectorBytes = Fs_CdSector.bytes;

        if (directorySectorBytes[0] != FILE_SYSTEM_ISO_BASE_RECORD_BYTES || directorySectorBytes[1] != 0) {
            _fsStartPayloadRead(FILE_SYSTEM_ISO_FALLBACK_DIRECTORY_SECTOR, FILE_SYSTEM_ISO_FALLBACK_DIRECTORY_SECTOR, directorySectorBytes, FILE_SYSTEM_PAYLOAD_RAW);
            while (Fs_CdOpStatus != (u8)FS_CD_STATUS_IDLE) {
                if (Fs_CdOpStatus == FILE_SYSTEM_CD_OPERATION_RESTART_REQUEST) {
                    cdSyncWaitForCommandCompletion();
                    goto restart;
                }
                VSync(0);
            }
        }
    }

    stageIndex = 0;
    do {
        Fs_StageCdfSectors[stageIndex] = 0;
        stageIndex++;
    } while (stageIndex < ARRAY_SIZE(Fs_StageCdfSectors));

    directoryRecord         = Fs_CdSector.bytes;
    directoryDone           = 0;
    D_8006AC30.field_4      = 0;
    Wip_SysFlags.discNumber = GAME_MAIN_DISC_UNKNOWN;
    D_8006AC30.startSector  = 0;
    Fs_Stage0HedSector      = 0;

    while ((directoryDone & 0xFF) == 0) {
        switch (directoryRecord[0]) {
            case FILE_SYSTEM_ISO_NAMED_RECORD_BYTES:
            case FILE_SYSTEM_ISO_NAMED_RECORD_BYTES + 2:
            case FILE_SYSTEM_ISO_NAMED_RECORD_BYTES + 4:
            case FILE_SYSTEM_ISO_NAMED_RECORD_BYTES + 6:
                nameByteIndex = 0;
                hasExtension  = 0;
                while (1) {
                    nameByte = directoryRecord[FILE_SYSTEM_ISO_NAME_OFFSET + (nameByteIndex & 0xFF)];
                    if (nameByte == '.') {
                        hasExtension = 1;
                        break;
                    }
                    if (nameByte == 0) {
                        break;
                    }
                    nameByteIndex++;
                }
                if ((hasExtension & 0xFF) != 0) {
                    s32 extensionOffset = nameByteIndex & 0xFF;
                    u8* extension       = &directoryRecord[FILE_SYSTEM_ISO_NAME_OFFSET + extensionOffset];

                    // Extents are two aligned little-endian halfwords in the ISO record.
                    if (strncmp(Fs_ExtensionCdf, (char*)extension, FILE_SYSTEM_ISO_EXTENSION_BYTES) == 0) {
                        u8 stageDigit;

                        stageDigit = directoryRecord[FILE_SYSTEM_ISO_NAME_OFFSET - 1 + extensionOffset] - '0';
                        Fs_StageCdfSectors[stageDigit] =
                            *(u16*)(directoryRecord + FILE_SYSTEM_ISO_EXTENT_LOW_OFFSET) + (*(u16*)(directoryRecord + FILE_SYSTEM_ISO_EXTENT_HIGH_OFFSET) << 16);
                    } else if (strncmp(Fs_ExtensionStr, (char*)extension, FILE_SYSTEM_ISO_EXTENSION_BYTES) == 0) {
                        D_8006AC30.startSector =
                            *(u16*)(directoryRecord + FILE_SYSTEM_ISO_EXTENT_LOW_OFFSET) + (*(u16*)(directoryRecord + FILE_SYSTEM_ISO_EXTENT_HIGH_OFFSET) << 16);
                    } else if (strncmp(Fs_Stage0HeaderName, (char*)(directoryRecord + FILE_SYSTEM_ISO_NAME_OFFSET), sizeof(Fs_Stage0HeaderName) - 1) == 0) {
                        Fs_Stage0HedSector =
                            *(u16*)(directoryRecord + FILE_SYSTEM_ISO_EXTENT_LOW_OFFSET) + (*(u16*)(directoryRecord + FILE_SYSTEM_ISO_EXTENT_HIGH_OFFSET) << 16);
                    } else if (strncmp(Fs_InitBitstreamName, (char*)(directoryRecord + FILE_SYSTEM_ISO_NAME_OFFSET), sizeof(Fs_InitBitstreamName) - 1) == 0) {
                        startupImageSectorCount = FILE_SYSTEM_STARTUP_IMAGE_SECTORS;
                        startupImageSector =
                            *(u16*)(directoryRecord + FILE_SYSTEM_ISO_EXTENT_LOW_OFFSET) + (*(u16*)(directoryRecord + FILE_SYSTEM_ISO_EXTENT_HIGH_OFFSET) << 16);
                    }
                }
            case FILE_SYSTEM_ISO_BASE_RECORD_BYTES:
            case FILE_SYSTEM_ISO_BASE_RECORD_BYTES + 2:
            case FILE_SYSTEM_ISO_BASE_RECORD_BYTES + 4:
            case FILE_SYSTEM_ISO_BASE_RECORD_BYTES + 6:
                directoryRecord += directoryRecord[0];
                break;
            default:
                directoryDone = 1;
                break;
        }
    }

    if (Fs_StageCdfSectors[1] != 0 || Fs_StageCdfSectors[2] != 0) {
        Wip_SysFlags.discNumber = GAME_MAIN_DISC_1;
    }
    if (Fs_StageCdfSectors[4] != 0 || Fs_StageCdfSectors[5] != 0) {
        Wip_SysFlags.discNumber = GAME_MAIN_DISC_2;
    }

    cdSyncWaitForCommandCompletion();

    // Startup alone requests the initial MDEC image; disc swaps omit it.
    if ((bootMode & 0xFF) != 0) {
        if (startupImageSector != 0) {
            Fs_ChunkMode    = FILE_SYSTEM_CHUNK_LOAD_NORMAL;
            D_8006ADF8      = 0;
            D5B498_8006EA1A = 0;
            D5B498_8006EBB0 = 0;
            D5B498_8006D850 = NULL;
            D5B498_8006D748 = FILE_SYSTEM_STREAM_DECODE_NEEDS_INPUT;
            D5B498_8006D858 = 1;
            D_8006C4D4      = Fs_CdSector.bytes;
            D_8006C4D4     += FS_SECTOR_BYTE_SIZE;
            mdecRequestImageVlcRebuild();

            _fsStartPayloadRead(startupImageSector, startupImageSector + startupImageSectorCount, NULL, FILE_SYSTEM_PAYLOAD_BACKGROUND);
            while (Fs_CdOpStatus != (u8)FS_CD_STATUS_IDLE) {
                if (Fs_CdOpStatus == FILE_SYSTEM_CD_OPERATION_RESTART_REQUEST) {
                    if (CdSync(1, NULL) == CdlDiskError) {
                        cdSyncWaitForReadableDisc(true);
                    }
                    goto restart;
                }
                VSync(0);
            }
            cdSyncWaitForCommandCompletion();
        }
    }

    if (Fs_Stage0HedSector != 0) {
        _fsStartScannedStage0HeaderRead();
    } else if ((bootMode & 0xFF) != 0) {
        goto restart;
    } else {
        Wip_SysFlags.discNumber = GAME_MAIN_DISC_UNKNOWN;
    }
}

u8 fsUploadImageChunk(const FsImageChunk* imageChunk, u8 ignoreGpuTimeLimit)
{
    enum {
        FILE_SYSTEM_IMAGE_UPLOAD_TIME_LIMIT_TICKS = 0x6E40,
    };
    u_long* drawResumeAddress;
    s32     ignoreTimeLimitAfterUpload;

    if (ResetRCnt(RCntCNT2) == 0) {
        return FILE_SYSTEM_IMAGE_UPLOAD_TIMER_FAILED;
    }

    // Suspend draw DMA before reusing the GPU for this upload.
    do {
        drawResumeAddress = BreakDraw();
        if (drawResumeAddress != FS_DRAW_BREAK_FAILED) {
            break;
        }
        if (GetRCnt(RCntCNT2) >= FILE_SYSTEM_IMAGE_UPLOAD_TIME_LIMIT_TICKS) {
            if (ignoreGpuTimeLimit == 0) {
                _fsResumeDrawing(drawResumeAddress);
                return FILE_SYSTEM_IMAGE_UPLOAD_RETRY;
            }
        }
    } while (1);

    // Relocate palette rows, then decode the payload into the upload buffer.
    _fsPrepareImageChunkUpload(imageChunk);
    fsDecompressImagePayload();

    if (D5B498_8006D748 == FILE_SYSTEM_IMAGE_DECODE_SCRATCH_BUSY) {
        _fsResumeDrawing(drawResumeAddress);
        return FILE_SYSTEM_IMAGE_UPLOAD_RETRY;
    }

    LoadImage2(&Fs_ImageRect, D5B498_8006D870);

    // An elapsed cutoff can reject the call even after the pixels reached VRAM.
    ignoreTimeLimitAfterUpload = ignoreGpuTimeLimit;
    do {
        while (IsIdleGPU(-1) != 0) {
        }
        if (GetRCnt(RCntCNT2) < FILE_SYSTEM_IMAGE_UPLOAD_TIME_LIMIT_TICKS) {
            break;
        }
        if (ignoreTimeLimitAfterUpload != 0) {
            break;
        }
        ContinueDraw(NULL, drawResumeAddress);
        return FILE_SYSTEM_IMAGE_UPLOAD_RETRY;
    } while (0);

    D_8006C4C8[D5B498_8006ADF4] = (u8)Fs_ImageRect.h;
    _fsResumeDrawing(drawResumeAddress);
    return FILE_SYSTEM_IMAGE_UPLOAD_COMPLETE;
}

void fsBeginImageColumns(const FsImageColumn* table)
{
    enum {
        FILE_SYSTEM_IMAGE_COLUMN_WIDTH           = 64,
        FILE_SYSTEM_IMAGE_COLUMN_STRIP_ROWS      = 32,
        FILE_SYSTEM_IMAGE_COLUMN_RELOCATION_ROWS = 128,
        FILE_SYSTEM_IMAGE_COLUMN_ROW_COUNT_MASK  = FILE_SYSTEM_IMAGE_COLUMN_ROWS_GIVEN - 1,
    };
    const FsImageColumn* column;
    s32                  columnIndex;

    // Retain the terminator: it also carries single-column height metadata.
    column      = table;
    columnIndex = 0;
    while (1) {
        Fs_WorkEntries[columnIndex].x          = column->x;
        Fs_WorkEntries[columnIndex].y          = column->y;
        Fs_WorkEntries[columnIndex].dataOffset = column->dataOffset;
        if (Fs_WorkEntries[columnIndex].x == FILE_SYSTEM_IMAGE_COLUMN_END) {
            break;
        }
        column++;
        columnIndex++;
    }

    // Relocate the initial strip; subsequent columns keep their recorded Y.
    if ((Fs_WorkEntries[0].y >= (u32)FILE_SYSTEM_IMAGE_COLUMN_ROWS) || (Fs_ChunkMode == 2)) {
        Fs_ImageRect.x = Fs_WorkEntries[0].x + D5B498_8006C233 * FILE_SYSTEM_IMAGE_COLUMN_WIDTH;
    } else {
        Fs_ImageRect.x = Fs_WorkEntries[0].x;
    }

    if (Fs_ChunkMode == 2) {
        Fs_ImageRect.y = Fs_WorkEntries[0].y + FILE_SYSTEM_IMAGE_COLUMN_RELOCATION_ROWS;
    } else {
        Fs_ImageRect.y = Fs_WorkEntries[0].y;
    }

    Fs_ImageRect.w  = FILE_SYSTEM_IMAGE_COLUMN_WIDTH;
    Fs_ImageRect.h  = FILE_SYSTEM_IMAGE_COLUMN_STRIP_ROWS;
    D5B498_8006ACD4 = FILE_SYSTEM_IMAGE_COLUMN_ROWS;

    Fs_ChunkReadPtr = (u8*)table + Fs_WorkEntries[0].dataOffset;

    // A terminator in the second record may set the single column's height.
    if (Fs_WorkEntries[1].x == FILE_SYSTEM_IMAGE_COLUMN_END) {
        if (Fs_WorkEntries[1].y == Fs_WorkEntries[1].x) {
            D5B498_8006ACD4 = FILE_SYSTEM_IMAGE_COLUMN_SHORT_ROWS;
        } else if (Fs_WorkEntries[1].y & FILE_SYSTEM_IMAGE_COLUMN_ROWS_GIVEN) {
            D5B498_8006ACD4 = Fs_WorkEntries[1].y & FILE_SYSTEM_IMAGE_COLUMN_ROW_COUNT_MASK;
        }
    }

    D5B498_8006ADE1                  = 1;
    D5B498_8006ADE0                  = 1;
    D5B498_8006D4E0[D5B498_8006ADF4] = 0;
}

/// Primes the next image strip's LZSS decode in the shared upload buffer.
///
/// Requires exclusive decoder/buffer use and prepared input as for
/// `fsDecompressStream`. Restarts the byte output cursor and bit reservoir,
/// discards any saved continuation, and starts history writes at ring index 1.
/// Preserves the compressed input cursor, input boundary, scratch history and
/// existing output bytes. The decoded strip must fit the 4480-byte buffer;
/// unwritten bytes remain available to the subsequent 4096-byte GPU upload.
/// Clears the new-strip latch so incomplete input resumes this same decode.
/// Does not read compressed bytes or start a GPU upload.
static inline void _fsBeginStripDecode(void)
{
    enum {
        FILE_SYSTEM_STRIP_LZSS_INITIAL_WRITE_INDEX = 1,
    };

    Fs_ChunkWritePtr = (u8*)D5B498_8006D870;
    // Reset the bit cursor and continuation without clearing history or pixels.
    D5B498_8006D748 = FILE_SYSTEM_STREAM_DECODE_NEEDS_INPUT;
    D5B498_8006EA1A = 0;
    D5B498_8006EBB0 = 0;
    D5B498_8006D850 = NULL;
    D5B498_8006D85A = 0;
    D5B498_8006D858 = FILE_SYSTEM_STRIP_LZSS_INITIAL_WRITE_INDEX;
    D5B498_8006ADE1 = false;
}

u8 fsUploadImageStrips(s32 inputMode)
{
    enum {
        FILE_SYSTEM_STRIP_GPU_TIME_LIMIT_TICKS = 0x6E40,
        FILE_SYSTEM_STRIP_HALFWORDS            = 64,
        FILE_SYSTEM_STRIP_ROWS                 = 32,
        FILE_SYSTEM_STRIP_MAX_ZERO_SCAN        = 6,
    };
    u_long*        drawResumeAddress;
    u_long*        drawBreakFailed;
    s32            residentInput;
    u8             zeroByteCount;
    u8*            paddingCursor;
    FsImageColumn* column;
    RECT*          columnRectangle;

    if (ResetRCnt(RCntCNT2) == 0) {
        return FILE_SYSTEM_IMAGE_STRIPS_TIMER_FAILED;
    }
    // Suspend draw DMA before sharing the GPU and upload buffer.
    drawBreakFailed = FS_DRAW_BREAK_FAILED;
    residentInput   = (u8)inputMode;
    for (;;) {
        drawResumeAddress = BreakDraw();
        if (drawResumeAddress != drawBreakFailed) {
            break;
        }
        if (GetRCnt(RCntCNT2) >= FILE_SYSTEM_STRIP_GPU_TIME_LIMIT_TICKS) {
            if (residentInput == 0) {
                _fsResumeDrawing(FS_DRAW_BREAK_FAILED);
                return FILE_SYSTEM_IMAGE_STRIPS_RETRY;
            }
        }
    }

    for (;;) {
        // Each strip has an independent LZSS cursor; incomplete input resumes it.
        if (D5B498_8006ADE1 != 0) {
            _fsBeginStripDecode();
        }
        fsDecompressStream();
        if (D5B498_8006D748 == FILE_SYSTEM_STREAM_DECODE_SCRATCH_BUSY) {
            _fsResumeDrawing(drawResumeAddress);
            return FILE_SYSTEM_IMAGE_STRIPS_RETRY;
        }
        if (D5B498_8006D748 == FILE_SYSTEM_STREAM_DECODE_NEEDS_INPUT) {
            _fsResumeDrawing(drawResumeAddress);
            if ((u8)inputMode == 0) {
                Fs_ChunkReadPtr = Fs_CdSector.bytes;
                if (GetRCnt(RCntCNT2) >= FILE_SYSTEM_STRIP_GPU_TIME_LIMIT_TICKS) {
                    return FILE_SYSTEM_IMAGE_STRIPS_RETRY;
                }
            }
            return FILE_SYSTEM_IMAGE_STRIPS_NEEDS_INPUT;
        }
        LoadImage2(&Fs_ImageRect, D5B498_8006D870);
        residentInput = (u8)inputMode;
        do {
            while (IsIdleGPU(-1) != 0) {
            }
            if (GetRCnt(RCntCNT2) < FILE_SYSTEM_STRIP_GPU_TIME_LIMIT_TICKS) {
                break;
            }
            if (residentInput != 0) {
                break;
            }
            ContinueDraw(NULL, drawResumeAddress);
            return FILE_SYSTEM_IMAGE_STRIPS_RETRY;
        } while (0);
        D5B498_8006ADE1  = 1;
        Fs_ImageRect.y  += FILE_SYSTEM_STRIP_ROWS;
        D5B498_8006ACD4 -= FILE_SYSTEM_STRIP_ROWS;
        if ((s16)D5B498_8006ACD4 <= 0) {
            if (Fs_WorkEntries[D5B498_8006ADE0].x == FILE_SYSTEM_IMAGE_COLUMN_END) {
                _fsResumeDrawing(drawResumeAddress);
                if ((u8)inputMode == 0) {
                    Fs_ChunkReadPtr = Fs_CdSector.bytes;
                    if (GetRCnt(RCntCNT2) >= FILE_SYSTEM_STRIP_GPU_TIME_LIMIT_TICKS) {
                        return FILE_SYSTEM_IMAGE_STRIPS_RETRY;
                    }
                }
                return FILE_SYSTEM_IMAGE_STRIPS_COMPLETE;
            }
            D5B498_8006D4E0[D5B498_8006ADF4]++;
            column = &Fs_WorkEntries[D5B498_8006ADE0];
            if (column->y >= (u32)FILE_SYSTEM_IMAGE_COLUMN_ROWS || Fs_ChunkMode == FILE_SYSTEM_CHUNK_LOAD_RELOCATE_IMAGES) {
                Fs_ImageRect.x = column->x + D5B498_8006C233 * FILE_SYSTEM_STRIP_HALFWORDS;
            } else {
                Fs_ImageRect.x = column->x;
            }
            D5B498_8006ACD4    = FILE_SYSTEM_IMAGE_COLUMN_ROWS;
            columnRectangle    = &Fs_ImageRect;
            columnRectangle->y = Fs_WorkEntries[D5B498_8006ADE0].y;
            columnRectangle->w = FILE_SYSTEM_STRIP_HALFWORDS;
            columnRectangle->h = FILE_SYSTEM_STRIP_ROWS;
            D5B498_8006ADE0++;
        }
        // A bounded zero scan separates strips without consuming the next token.
        zeroByteCount = 0;
        paddingCursor = Fs_ChunkReadPtr;
        do {
            {
                u8 paddingByte = *paddingCursor++;
                if (paddingByte != 0) {
                    break;
                }
            }
            Fs_ChunkReadPtr++;
            zeroByteCount++;
            if (Fs_ChunkReadPtr >= D_8006CCD8 || zeroByteCount >= (u32)FILE_SYSTEM_STRIP_MAX_ZERO_SCAN) {
                _fsResumeDrawing(drawResumeAddress);
                if ((u8)inputMode == 0) {
                    Fs_ChunkReadPtr = D_8006CCD8 - sizeof(Fs_CdSector);
                    if (GetRCnt(RCntCNT2) >= FILE_SYSTEM_STRIP_GPU_TIME_LIMIT_TICKS) {
                        return FILE_SYSTEM_IMAGE_STRIPS_RETRY;
                    }
                }
                return FILE_SYSTEM_IMAGE_STRIPS_NEEDS_INPUT;
            }
        } while (1);
        D5B498_8006D748 = FILE_SYSTEM_STREAM_DECODE_NEEDS_INPUT;
    }
}

void cdSyncWaitForCommandCompletion(void)
{
    u8  commandComplete;
    s32 syncStatus;
    u8  modeParameters[8];
    u8  commandResult[8];

    commandComplete = 0;
    do {
        syncStatus = CdSync(1, NULL);
        switch (syncStatus) {
            case CdlNoIntr:
                break;
            case CdlComplete:
                commandComplete = 1;
                break;
            case CdlDiskError:
                // Leave read mode before waiting for a ready CD-ROM.
                CdSync(0, NULL);
                modeParameters[0] = 0;
                CdControlB(CdlSetmode, modeParameters, NULL);
                VSync(3);

                do {
                    do {
                        CdControlB(CdlNop, NULL, commandResult);
                    } while (commandResult[0] & CdlStatShellOpen);
                } while (CdDiskReady(0) != CdlComplete || CdGetDiskType() != CdlCdromFormat);

                // Resume double-speed reads with sector headers.
                modeParameters[0] = CdlModeSpeed | CdlModeSize1;
                CdControlB(CdlSetmode, modeParameters, NULL);
                VSync(3);
        }
    } while (commandComplete == 0);
}

void Fs_RetryReadN(void)
{
    CdlLOC loc[2];
    u8     ctrlParam[8];
    u8     ctrlResult[8];
    u8*    ctrlParamPtr;

    if (CdSync(1, NULL) == CdlDiskError) {
        // Wait for the command to finish and reset the operation mode.
        CdSync(0, NULL);
        ctrlParam[0] = 0;
        ctrlParamPtr = ctrlParam;
        CdControlB(CdlSetmode, ctrlParamPtr, NULL);
        VSync(3);

        // Wait until the CD shell is closed with a valid disk.
        do {
            do {
                CdControlB(CdlNop, NULL, ctrlResult);
            } while ((ctrlResult[0] & CdlStatShellOpen) != 0);
        } while (CdDiskReady(0) != CdlComplete || CdGetDiskType() != CdlCdromFormat);

        // Enable double speed and sector header.
        ctrlParamPtr[0] = CdlModeSpeed | CdlModeSize1;
        CdControlB(CdlSetmode, ctrlParam, NULL);
        VSync(3);
    }

    CdIntToPos(Fs_ReqSector, loc);
    CdControlF(CdlReadN, &loc[0].minute);
    if (Fs_CdOpStatus == 0x40) {
        CdSyncCallback(_fsReadNSyncCallback);
        Fs_VBlank      = VSync(-1);
        Fs_CdOpStatus += 1;
    }
}

u8 cdSyncWaitForDiscSwap(void)
{
    enum {
        CD_SYNC_VOLUME_DESCRIPTOR_SECTOR = 16,
        CD_SYNC_TOC_RETRY_VBLANKS        = 30,
    };
    s32    tocCommandAccepted;
    u8     modeParameters[8];
    u8     commandResult[8];
    CdlLOC volumeLocation[2];

    VSync(0);

    // Observe a full tray cycle before probing the replacement disc.
    do {
        CdControlB(CdlNop, NULL, commandResult);
    } while ((commandResult[0] & CdlStatShellOpen) == 0);

    do {
        CdControlB(CdlNop, NULL, commandResult);
    } while ((commandResult[0] & CdlStatShellOpen) != 0);

    while ((commandResult[0] & CdlStatStandby) == 0) {
        CdControlB(CdlNop, NULL, commandResult);
    }

    // Wait for the new TOC and spindle readiness before starting a data read.
    tocCommandAccepted = CdControlB(CdlGetTN, NULL, commandResult);
    while (commandResult[0] != CdlStatStandby || tocCommandAccepted == CdlNoIntr) {
        VSync(CD_SYNC_TOC_RETRY_VBLANKS);
        tocCommandAccepted = CdControlB(CdlGetTN, NULL, commandResult);
    }

    modeParameters[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, modeParameters, NULL);
    VSync(3);

    // Probe the ISO volume sector using ordinary 2048-byte data mode.
    CdIntToPos(CD_SYNC_VOLUME_DESCRIPTOR_SECTOR, volumeLocation);
    CdControlB(CdlReadN, &volumeLocation[0].minute, commandResult);

    // The response-bit tests all return the same failure byte in the binary.
    if (CdSync(0, commandResult) == CdlDiskError) {
        if (commandResult[0] & CdlStatError) {
            if (commandResult[1] & 0x40) {
                return CD_SYNC_DISC_SWAP_ERROR;
            }
        }
        return CD_SYNC_DISC_SWAP_ERROR;
    }

    // A successful probe leaves the drive paused in the filesystem's read mode.
    CdControlB(CdlPause, NULL, commandResult);
    modeParameters[0] = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, modeParameters, NULL);
    return CD_SYNC_DISC_SWAP_COMPLETE;
}

void fsStartPayloadRead(s32 startSector, s32 endSector, void* destination, u8 payloadPhase)
{
    _fsStartPayloadRead(startSector, endSector, destination, payloadPhase);
}

static void Fs_ReadSector(s32 sector)
{
    CdlLOC loc[2];

    if (CdSync(1, NULL) == CdlDiskError) {
        cdSyncWaitForReadableDisc(true);
    }

    Fs_CdOpStatus   = 0;
    Fs_ChunkEndFlag = -1;
    Fs_ReqSector    = sector;
    Fs_VBlank       = VSync(-1);
    if (Fs_SeekSector == sector) {
        Fs_SeekSector = 0;
        Fs_Streaming  = false;
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(_fsChunkReadyCallback);
    } else {
        Fs_SeekSector = 0;
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(_fsReadNSyncCallback);
    }
}

void cdSyncWaitForReadableDisc(s8 includeSectorHeader)
{
    u8 modeParameters[8];
    u8 commandResult[8];

    // Wait for the command to finish and reset the operation mode.
    CdSync(0, NULL);
    modeParameters[0] = 0;
    CdControlB(CdlSetmode, modeParameters, NULL);
    VSync(3);

    // Wait for a closed shell with a readable CD-ROM; no opening is required.
    do {
        do {
            CdControlB(CdlNop, NULL, commandResult);
        } while ((commandResult[0] & CdlStatShellOpen) != 0);
    } while (CdDiskReady(0) != CdlComplete || CdGetDiskType() != CdlCdromFormat);

    // Enable double speed and optionally also the sector header.
    if (includeSectorHeader != 0) {
        modeParameters[0] = CdlModeSpeed | CdlModeSize1;
    } else {
        modeParameters[0] = CdlModeSpeed;
    }
    CdControlB(CdlSetmode, modeParameters, NULL);
    VSync(3);
}

/// Starts a seek to an absolute CD sector for a later file read.
///
/// The request must be serialized with other filesystem operations. A pending
/// drive error is recovered first. The target is retained so a following read
/// at that sector can omit its location parameter; completion is asynchronous.
static void _fsSeekToSector(s32 absoluteSector)
{
    CdlLOC seekLocation;

    Fs_CdOpStatus = FILE_SYSTEM_CD_OPERATION_PENDING;
    if (CdSync(1, NULL) == CdlDiskError) {
        cdSyncWaitForReadableDisc(true);
    }

    Fs_SeekSector = absoluteSector;
    CdIntToPos(absoluteSector, &seekLocation);
    CdControlF(CdlSeekL, &seekLocation.minute);
    CdSyncCallback(_fsSeekSyncCallback);
    Fs_VBlank = VSync(-1);
}

void fsInitFolderTable(s32 unusedStageIndex)
{
    u32                   sectorOffset;
    FsCdfFolderListEntry* folderEntry;

    Fs_FolderTableLen = 0;
    folderEntry       = Fs_CdSector.folderList.entries;

    // The folder table fills the CDF's first sector, so the first folder
    // starts at sector 1. Each sector count places the folder after it.
    sectorOffset = 1;
    while (true) {
        if (folderEntry->sectorCount == FS_CDF_FOLDER_CANARY) {
            return;
        }

        Fs_FolderTable[Fs_FolderTableLen].folderId     = folderEntry->folderId;
        Fs_FolderTable[Fs_FolderTableLen].sectorOffset = sectorOffset;
        Fs_FolderTableLen                             += 1;

        sectorOffset += folderEntry->sectorCount;
        folderEntry  += 1;
    }
}

void fsStartStage0HeaderRead(void)
{
    CdlLOC headerLocation;

    // Discard active append counts; indexed tables are overwritten by the HED.
    Fs_CdOpStatus       = FILE_SYSTEM_CD_OPERATION_PENDING;
    Fs_FileTableLen     = 0;
    Fs_FileTableCat2Len = 0;
    Fs_FileTableCat4Len = 0;
    Fs_FileTableCat1Len = 0;
    Fs_FileTableCat3Len = 0;

    // Publish the expected sector before enabling its ready callback.
    Fs_ReqSector = Fs_Stage0HedSector;
    CdIntToPos(Fs_Stage0HedSector, &headerLocation);
    CdControlF(CdlReadN, &headerLocation.minute);
    CdReadyCallback(_fsStage0HeaderReadyCallback);
    Fs_VBlank = VSync(-1);
}

/// Selects sector processing after a chunk or sound-bank ReadN command starts.
///
/// Every interrupt except CdlDiskError accepts the read. Chunk resumption
/// enables streaming; a fresh chunk read disables it, while a sound-bank read
/// initializes the loader before installing its ready callback. Result bytes
/// are ignored. Success resets the timeout/error counters and removes this
/// sync callback; a disk error selects the filesystem's soft restart path.
static void _fsReadNSyncCallback(u8 interruptStatus, u8* unusedResult)
{
    if (interruptStatus != CdlDiskError) {
        if (Fs_CdOpStatus == FILE_SYSTEM_CD_OPERATION_RESUMING_CHUNK) {
            Fs_CdOpStatus = FILE_SYSTEM_CD_OPERATION_PENDING;
            Fs_Streaming  = true;
            CdReadyCallback(_fsChunkReadyCallback);
        } else if (D5B498_8006ACC8 == false) {
            Fs_Streaming = false;
            CdReadyCallback(_fsChunkReadyCallback);
        } else {
            sndLoadBeginSectorLoad(&Fs_CdSector);
            CdReadyCallback(fsSoundBankReadyCallback);
        }

        Fs_VBlank = VSync(-1);
        CdSyncCallback(NULL);
        Fs_CdErrorCount = 0;
    } else {
        _fsHandleCdError(FS_ERROR_SOFT);
    }
}

/// Switches a started raw/payload read to sector processing, or restarts it.
///
/// Used as a ready callback after a reused seek or a sync callback after ReadN.
/// Every interrupt except CdlDiskError clears errors and enables payload reads;
/// no sector bytes are consumed here. SDK result bytes are unused.
static void _fsPayloadReadStartedCallback(u8 interruptStatus, u8* unusedResult)
{
    if (interruptStatus != CdlDiskError) {
        Fs_VBlank       = VSync(-1);
        Fs_CdErrorCount = 0;
        Fs_Streaming    = 1;
        CdReadyCallback(_fsChunkReadyCallback);
        CdSyncCallback(NULL);
    } else {
        _fsHandleCdError(FS_ERROR_SOFT);
    }
}

/// Completes a filesystem seek, or leaves the request selected for restart.
///
/// Accepts only the SDK's `CdlComplete` interrupt. Its result bytes are unused.
/// Success clears the sync callback and error count but retains the seek target.
static void _fsSeekSyncCallback(u8 interruptStatus, u8* unusedResult)
{
    if (interruptStatus == CdlComplete) {
        Fs_CdOpStatus = FS_CD_STATUS_IDLE;
        CdSyncCallback(NULL);
        Fs_CdErrorCount = 0;
    } else {
        _fsHandleCdError(FS_ERROR_SOFT);
    }
}

/// Pauses a failed transfer and selects chunk resumption or request restart.
///
/// `FS_ERROR_HARD` (2) retains sound-load state and selects resumption at
/// `Fs_ReqSector`. All other values, normally `FS_ERROR_SOFT` (0), release that
/// state and select a fresh request. Every call increments the byte error count
/// and removes both CD callbacks before pausing; it does not reissue a read.
static void _fsHandleCdError(u8 recoveryMode)
{
    Fs_CdErrorCount += 1;
    CdReadyCallback(NULL);
    CdSyncCallback(NULL);

    if (recoveryMode == FS_ERROR_HARD) {
        Fs_CdOpStatus = FILE_SYSTEM_CD_OPERATION_RESUME_CHUNK;
    } else {
        sndLoadTeardown();
        Fs_CdOpStatus = FILE_SYSTEM_CD_OPERATION_RESTART_REQUEST;
    }

    CdControlF(CdlPause, NULL);
}

/// Waits for GPU upload completion before resuming the interrupted draw DMA.
///
/// `resumeAddress` is the opaque `BreakDraw` result, including its -1 failure
/// sentinel. `ContinueDraw` forwards it to the DMA register, without a CPU
/// dereference. Both successful uploads and aborted transfers use this path.
static void _fsResumeDrawing(u_long* resumeAddress)
{
    while (IsIdleGPU(-1) != 0) {
    }
    ContinueDraw(NULL, resumeAddress);
}

u8* fsGetChunkPayload(void)
{
    return Fs_CdSector.chunk.data.bytes;
}

void fsAbortTimedOutOperation(void)
{
    enum { FILE_SYSTEM_READ_TIMEOUT_VBLANKS = 180 };
    u8 pauseResult[8];

    if (Fs_CdOpStatus != FILE_SYSTEM_CD_OPERATION_PENDING && Fs_CdOpStatus != FILE_SYSTEM_CD_OPERATION_RESUMING_CHUNK) {
        return;
    }

    if (VSync(-1) <= Fs_VBlank + FILE_SYSTEM_READ_TIMEOUT_VBLANKS) {
        return;
    }

    Fs_VBlank     = VSync(-1);
    Fs_CdOpStatus = FILE_SYSTEM_CD_OPERATION_RESTART_REQUEST;
    CdFlush();
    sndLoadTeardown();
    CdReadyCallback(NULL);
    CdSyncCallback(NULL);
    CdControlB(CdlPause, NULL, pauseResult);
}

void cdSyncStopDisc(void)
{
    u8 modeParameters[9];

    modeParameters[0] = 0;
    modeParameters[8] = 0;
    CdControlB(CdlSetmode, modeParameters, NULL);

    VSync(3);
    CdControlB(CdlStop, NULL, NULL);
}

s32 fsGetRequiredStageDisc(void)
{
    u8 stage;

    stage = gGameSession->location.loc.stage;
    if (Fs_StageCdfSectors[stage] == 0) {
        if (stage == GAME_STAGE_ACROPOLIS || stage == GAME_STAGE_DRYFIELD) {
            return GAME_MAIN_DISC_1;
        }
        if (stage == GAME_STAGE_MINE_SHELTER || stage == GAME_STAGE_SHELTER_NEO_ARK) {
            return GAME_MAIN_DISC_2;
        }
    }
    return GAME_MAIN_DISC_UNKNOWN;
}
