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

// Args used by Fs_OnCdError
#define FS_ERROR_SOFT 0x0

#define FS_ERROR_HARD 0x2

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
/// Filled by `Fs_BuildFolderTables` from the folder file list in `Fs_CdSector`.
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

static void Fs_ResetBootLoadState(void);

static void Fs_CdReadyCb(u8 status, u8* result);

static u8 Fs_ProcessChunkHeader(void);

static u8 Fs_ProcessChunkData(void);

/// Starts a ReadN of `sector` onwards into `dest`, reusing the position of a
/// preceding seek when it targeted the same sector.
static inline void _fsStartRead(s32 sector, s32 endSector, u8* dest, u8 phase);

static void Fs_InitStage0TablesCb(u8 status, u8* result);

static void Fs_ReadSector(s32 sector);

static void Fs_SeekToPos(s32 sector);

static void Fs_ReadNSyncCb(u8 status, u8* result);

static void Fs_ReadNReadyCb(u8 status, u8* result);

static void Fs_SeekToPosCb(u8 status, u8* result);

static void Fs_OnCdError(u8 arg0);

static void Fs_ContinueDrawing(u_long* ot);

/// Unreferenced.
static s32 D_8005EBC0 = 0;

static void Fs_ResetBootLoadState(void)
{
    Fs_BootLoadPhase           = 0;
    gCdCmdQueue.bootLoadActive = 0;
}

void Fs_BeginBootLoad(u8* arg0, s16 arg1)
{
    gCdCmdQueue.bootLoadActive = 1;
    Fs_LoadParams.stage        = arg0[3];
    Fs_LoadParams.area         = arg0[2];
    D5B498_8006ACC0            = arg1;

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
            if (CdCmd_IsSlotEmpty(Fs_BootLoadSlot)) {
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
                Fs_SeekToPos(sector);
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

static void Fs_CdReadyCb(u8 status, u8* result)
{
    CdlLOC currLoc[3];
    s32    currPos;
    u8     ret;

    if (status != CdlDiskError) {
        Fs_VBlank = VSync(-1);
        CdGetSector(currLoc, 3);
        Fs_CurrSector = currPos = CdPosToInt(currLoc);

        if (currPos != Fs_ReqSector) {
            if ((Fs_Streaming != 0) && (Fs_LoadPhase != 6)) {
                Fs_OnCdError(FS_ERROR_HARD);
                return;
            }
            Fs_OnCdError(FS_ERROR_SOFT);
            return;
        }

        Fs_ReqSector = currPos + 1;
        if (Fs_Streaming == 0) {
            ret = Fs_ProcessChunkHeader();
        } else {
            ret = Fs_ProcessChunkData();
        }
    } else {
        Fs_OnCdError(FS_ERROR_SOFT);
        return;
    }

    if (ret != 0) {
        CdControlF(CdlPause, NULL);
        CdReadyCallback(NULL);
        D5B498_8006C234 = 0;
        D5B498_8006C233 = 0;
        D5B498_8006ADF4 = 0;
        Fs_ChunkMode    = 0;
        Fs_CdOpStatus   = FS_CD_STATUS_IDLE;
    }
}

static u8 Fs_ProcessChunkHeader(void)
{
    s32               i = 0;
    FsCdfChunkHeader* hdr;
    s32               status;

    // Bound the read by the chunk's sector count and the valid bytes in this sector.
    CdGetSector(&Fs_CdSector, 0x200);
    D_8006C4D4        = Fs_CdSector.bytes;
    hdr               = &Fs_CdSector.chunk.header;
    Fs_ChunkWritePtr  = hdr->loadAddr;
    Fs_ChunkEndSector = Fs_ReqSector - 1 + hdr->sectorCount;
    Fs_ChunkEndFlag   = hdr->endFlag;
    D_8006C4D4       += hdr->sectorLen;

    switch (Fs_CdSector.chunk.header.type) {
        case FILE_SYSTEM_CHUNK_PACKAGE:
            if (Fs_ChunkWritePtr == NULL) {
                break;
            }
            if (Fs_ChunkMode == 1 || Fs_ChunkMode == 4) {
                Fs_LoadPhase = 0xFF;
                return 1;
            }
            D5B498_8006EA1A = 0;
            D5B498_8006EBB0 = 0;
            D5B498_8006D858 = 1;
            D5B498_8006D850 = 0;
            D5B498_8006D748 = 0;
            Fs_ChunkReadPtr = Fs_CdSector.chunk.data.bytes;
            Fs_DecompressChunk();
            if (D5B498_8006D748 == 0xFFFF) {
                Fs_OnCdError(0);
                break;
            }
            if (D5B498_8006D748 != 0) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = 0xFF;
                    return 1;
                }
                break;
            }
            Fs_LoadPhase = 1;
            Fs_Streaming = 1;
            break;

        case FILE_SYSTEM_CHUNK_IMAGE:
            Fs_CopyWorkEntries((FsImageColumn*)Fs_CdSector.chunk.data.bytes);
            status = Fs_LoadImageStrip(0);
            if (status == 0xFF || status == 0x7F) {
                Fs_OnCdError(0);
                break;
            }
            if (status == 1) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = 0xFF;
                    return 1;
                }
            } else {
                Fs_LoadPhase = 2;
                Fs_Streaming = 1;
            }
            break;

        case FILE_SYSTEM_CHUNK_CLUT:
            if (Fs_ChunkEndSector == Fs_ReqSector) {
                status = Fs_LoadImageChunk((FsImageChunk*)Fs_CdSector.chunk.data.bytes, 0);
                if (status == 0xFF || status == 0x7F) {
                    Fs_OnCdError(0);
                    break;
                }
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = 0xFF;
                    return 1;
                }
            } else {
                Fs_LoadPhase = 3;
                Fs_Streaming = 1;
            }
            break;

        case FILE_SYSTEM_CHUNK_RAW: {
            u32* src;
            u32* dst;
            if (Fs_ChunkMode == 1 || Fs_ChunkMode == 4) {
                Fs_LoadPhase = 0xFF;
                return 1;
            }
            src = (u32*)&Fs_CdSector.chunk.data.bytes[i];
            dst = (u32*)Fs_ChunkWritePtr;
            for (i = 0; i < ARRAY_SIZE(Fs_CdSector.chunk.data.words); i++) {
                dst[i] = src[i];
            }
            Fs_ChunkWritePtr += sizeof(Fs_CdSector.chunk.data.bytes);
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = 0xFF;
                    return 1;
                }
            } else {
                Fs_LoadPhase = 0;
                Fs_Streaming = 1;
            }
            break;
        }

        case FILE_SYSTEM_CHUNK_BUNDLE: {
            _FsCdfResourceEntry* entry;
            if (Fs_ChunkMode == 1 || Fs_ChunkMode == 4) {
                Fs_LoadPhase = 0xFF;
                return 1;
            }
            entry = (_FsCdfResourceEntry*)Fs_CdSector.chunk.data.bytes;
            // Publish resource destinations before streaming the bundle's payload.
            for (i = 0; i < ARRAY_SIZE(D_8006C338); i++) {
                D_8006C338[i].kind = entry->kind;
                D_8006C338[i].data = entry->destination;
                if (entry->redirectDestination != NULL) {
                    Fs_LoadRedirect.enabled               = 1;
                    Fs_LoadRedirect.sectorsBeforeRedirect = entry->sectorsBeforeRedirect;
                    Fs_LoadRedirect.destination           = entry->redirectDestination;
                }
                entry++;
            }
            Fs_LoadRedirect.sectorsRead = 0;
            Fs_LoadPhase                = 4;
            Fs_Streaming                = 1;
            break;
        }

        case FILE_SYSTEM_CHUNK_BACKGROUND: {
            u8* buf;
            if (Fs_ChunkMode != 3) {
                mdecRequestImageVlcRebuild();
                buf = (u8*)Fs_ImgBuffers;
                for (D_8006ADF8 = 0; (u32)D_8006ADF8 < sizeof(Fs_CdSector.chunk.data.bytes); D_8006ADF8++) {
                    buf[D_8006ADF8] = Fs_CdSector.chunk.data.bytes[D_8006ADF8];
                }
            }
            Fs_LoadPhase = 5;
            Fs_Streaming = 1;
            break;
        }

        case FILE_SYSTEM_CHUNK_MUSIC:
            if (Fs_ChunkMode == 4 || Fs_ChunkMode == 5) {
                Fs_LoadPhase = 0xFF;
                Fs_Streaming = 1;
                break;
            }
            SndLoad_BeginFromBuffer(0, Fs_CdSector.bytes);
            status = SndLoad_FeedSector(Fs_CdSector.bytes);
            if (status == SOUND_LOAD_PHASE_DONE) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = 0xFF;
                    return 1;
                }
            } else if (status == -1) {
                Fs_OnCdError(0);
                break;
            } else {
                Fs_LoadPhase = 6;
                Fs_Streaming = 1;
            }
            break;

        case FILE_SYSTEM_CHUNK_TEXT:
            if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                Fs_LoadPhase = 0xFF;
                return 1;
            }
            break;

        default:
            if (Fs_ChunkEndSector == Fs_ReqSector) {
                if (Fs_ChunkEndFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = 0xFF;
                    return 1;
                }
            } else {
                Fs_LoadPhase = 0xFF;
                Fs_Streaming = 1;
            }
            break;
    }
    return 0;
}

static u8 Fs_ProcessChunkData(void)
{
    s32  status;
    s32  endFlag;
    s32* sizes;
    s32  ff;

    switch (Fs_LoadPhase) {
        case 0:
            CdGetSector(Fs_ChunkWritePtr, 0x200);
            Fs_ChunkWritePtr += 0x800;
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                endFlag = Fs_ChunkEndFlag;
                if (endFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endFlag;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case 1:
            CdGetSector(Fs_CdSector.bytes, 0x200);
            Fs_ChunkReadPtr = Fs_CdSector.bytes;
            Fs_DecompressChunk();
            if (D5B498_8006D748 == 0xFFFF) {
                Fs_OnCdError(0);
            } else if (D5B498_8006D748 != 0 || (u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                switch (D5B498_8006ADF4 - 1) {
                    case 0:
                        Fs_ChunkOutputSizes[0] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase0;
                        break;
                    case 1:
                        Fs_ChunkOutputSizes[1] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase1;
                        break;
                    case 2:
                    case 7:
                        Fs_ChunkOutputSizes[2] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase2;
                        break;
                    case 3:
                        sizes                  = Fs_ChunkOutputSizes;
                        sizes[1]               = -1;
                        Fs_ChunkOutputSizes[0] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase0;
                        break;
                    case 4: {
                        s32* p;
                        p                      = Fs_ChunkOutputSizes;
                        p[1]                   = -1;
                        p[2]                   = -1;
                        Fs_ChunkOutputSizes[0] = Fs_ChunkWritePtr - (u8*)Fs_ActorLoadBase0;
                        break;
                    }
                }
                endFlag = Fs_ChunkEndFlag;
                if (endFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endFlag;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case 2:
            CdGetSector(Fs_CdSector.bytes, 0x200);
            status = Fs_LoadImageStrip(0);
            ff     = 0xFF;
            if (status == ff) {
                Fs_ReqSector--;
                Fs_OnCdError(2);
            } else if (status == 0x7F) {
                Fs_OnCdError(0);
            } else if (status == 1 || (u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                endFlag = Fs_ChunkEndFlag;
                if (endFlag == ff) {
                    Fs_LoadPhase = endFlag;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case 3:
            CdGetSector(D_8006CCD8, 0x200);
            // The image header is the start of the previous sector's payload, contiguous with this continuation sector.
            status = Fs_LoadImageChunk((FsImageChunk*)(D_8006CCD8 - sizeof(Fs_CdSector.chunk.data.bytes)), 0);
            ff     = 0xFF;
            if (status == ff) {
                Fs_ReqSector--;
                Fs_OnCdError(2);
            } else if (status == 0x7F) {
                Fs_OnCdError(0);
            } else {
                endFlag = Fs_ChunkEndFlag;
                if (endFlag == ff) {
                    Fs_LoadPhase = endFlag;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case 4:
            CdGetSector(Fs_ChunkWritePtr, 0x200);
            Fs_ChunkWritePtr += 0x800;
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                endFlag = Fs_ChunkEndFlag;
                if (endFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endFlag;
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
        case 5:
            if (Fs_ChunkMode != 3) {
                CdGetSector((u8*)Fs_ImgBuffers + D_8006ADF8, 0x200);
                D_8006ADF8 += 0x800;
            }
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                if (Fs_ChunkMode != 3) {
                    Mdec_BeginDecode(Fs_ImgBuffers);
                }
                endFlag = Fs_ChunkEndFlag;
                if (endFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endFlag;
                    return 1;
                }
                Fs_Streaming = 0;
            }
            break;
        case 6:
            CdGetSector(Fs_CdSector.bytes, 0x200);
            status = SndLoad_FeedSector(Fs_CdSector.bytes);
            if (status == SOUND_LOAD_PHASE_DONE) {
                endFlag = Fs_ChunkEndFlag;
                if (endFlag == FILE_SYSTEM_CHUNK_LAST) {
                    Fs_LoadPhase = endFlag;
                    return 1;
                }
                Fs_Streaming = 0;
            } else if (status == -1) {
                Fs_OnCdError(0);
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

void Fs_SelectStage(s32 stageIdx)
{
    CdlLOC loc[2];
    s32    sector;
    u8*    dest;

    Fs_ChunkMode    = 0;
    D5B498_8006ADF4 = 0;
    sector          = Fs_StageCdfSectors[(u8)stageIdx];
    dest            = Fs_CdSector.bytes;

    if (CdSync(1, NULL) == CdlDiskError) {
        Fs_WaitDiskReset(1);
    }

    Fs_CdOpStatus     = 0;
    Fs_ChunkEndFlag   = -1;
    Fs_LoadPhase      = 0;
    Fs_ReqSector      = sector;
    Fs_ChunkEndSector = sector;
    Fs_ChunkWritePtr  = dest;
    Fs_VBlank         = VSync(-1);

    if (Fs_SeekSector == sector) {
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    }

    Fs_VBlank = VSync(-1);
}

/// Starts a ReadN of `sector` onwards into `dest`, reusing the position of a
/// preceding seek when it targeted the same sector.
static inline void _fsStartRead(s32 sector, s32 endSector, u8* dest, u8 phase)
{
    CdlLOC loc[2];

    if (CdSync(1, NULL) == CdlDiskError) {
        Fs_WaitDiskReset(true);
    }

    Fs_CdOpStatus     = 0;
    Fs_ChunkEndFlag   = -1;
    Fs_LoadPhase      = phase;
    Fs_ReqSector      = sector;
    Fs_ChunkEndSector = endSector;
    Fs_ChunkWritePtr  = dest;
    Fs_VBlank         = VSync(-1);
    if (Fs_SeekSector == sector) {
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    }
}

void Fs_PrepareFolderLoad(s32 arg0, s32 arg1, s32 arg2)
{
    u16 i;
    s32 folderId;
    s32 sector;

    Fs_LoadRedirect.enabled               = 0;
    Fs_LoadRedirect.sectorsRead           = 0;
    Fs_LoadRedirect.sectorsBeforeRedirect = 0;
    Fs_LoadRedirect.destination           = NULL;
    Fs_ChunkMode                          = 0;
    D5B498_8006ADF4                       = 0;

    for (i = 0; i < ARRAY_SIZE(D_8006C338); i++) {
        D_8006C338[i].kind = FILE_SYSTEM_RESOURCE_NONE;
    }

    for (i = 0; i < ARRAY_SIZE(Stream_Slots); i++) {
        Stream_Slots[i].startSector = 0;
    }

    D_8006ADE2 = 0;
    folderId   = ((u8)arg1 * 100) + (u8)arg2;

    for (i = 0; i < Fs_FolderTableLen; i++) {
        if (folderId == Fs_FolderTable[i].folderId) {
            break;
        }
    }

    Fs_VBlank = VSync(-1);

    sector = Fs_FolderTable[i].sectorOffset + Fs_StageCdfSectors[(u8)arg0];
    _fsStartRead(sector, sector, Fs_CdSector.bytes, 0);
}

/// Copies one stream descriptor byte by byte.
static inline void Fs_CopyStreamSlot(StreamSlot* destination, StreamSlot* source)
{
    s32 k;
    u8* src;
    u8* dst;

    src = (u8*)source;
    dst = (u8*)destination;
    for (k = 0; (u16)k < sizeof(*destination); k++) {
        dst[k & 0xFFFF] = src[k & 0xFFFF];
    }
}

void Fs_BuildFolderTables(s32 arg0, s32 arg1, s32 arg2)
{
    enum { FILE_SYSTEM_FOLDER_STREAM_TABLE_OFFSET = 0x514 };
    STATIC_ASSERT(sizeof(Fs_CdSector.fileList) <= FILE_SYSTEM_FOLDER_STREAM_TABLE_OFFSET, fs_file_list_precedes_stream_table);
    s32 i;
    s32 j;
    s32 folderId;
    union {
        FsCdfFile*    file;
        _FsCdfFolder* folder;
    } files;
    s32*          table;
    s32           offset;
    _FsCdfFolder* folder;
    StreamSlot*   sourceStreams;
    StreamSlot*   destinationStreams;

    i        = 0;
    folderId = ((u8)arg1 * 100) + (u8)arg2;
    for (; (u16)i < Fs_FolderTableLen; i++) {
        if (folderId == Fs_FolderTable[i & 0xFFFF].folderId) {
            break;
        }
    }

    files.file = Fs_CdSector.fileList;
    j          = 0;
    table      = D_8006C158;
    {
        _FsCdfFolder* sp = Fs_FolderTable;
        folder           = sp + (i & 0xFFFF);
    }
    for (;;) {
        offset = files.file[j & 0xFFFF].sectorOffset;
        if (offset != 0) {
            table[files.file[j & 0xFFFF].fileId] = offset + folder->sectorOffset;
            j                                   += 1;
        } else {
            break;
        }
    }

    // Resolve the stream folder independently of the file-load folder.
    i             = 0;
    folderId      = ((u8)arg1 * 100) + 1;
    sourceStreams = (StreamSlot*)(Fs_CdSector.bytes + FILE_SYSTEM_FOLDER_STREAM_TABLE_OFFSET);
    for (; (u16)i < Fs_FolderTableLen; i++) {
        if (folderId == Fs_FolderTable[i & 0xFFFF].folderId) {
            break;
        }
    }

    j = 0;
    {
        _FsCdfFolder* sp = Fs_FolderTable;
        files.folder     = sp + (i & 0xFFFF);
    }
    {
        s32* sp = Fs_StageCdfSectors;
        table   = sp + (u8)arg0;
    }
    // Publish complete descriptors with absolute CD sectors.
    destinationStreams = Stream_Slots;
    for (;;) {
        if (sourceStreams[j & 0xFFFF].key.word != STREAM_KEY_TERMINATOR) {
            sourceStreams[j & 0xFFFF].startSector += files.folder->sectorOffset + *table;
            Fs_CopyStreamSlot(&destinationStreams[j & 0xFFFF], &sourceStreams[j & 0xFFFF]);
            j += 1;
        } else {
            break;
        }
    }
}

static void Fs_InitStage0TablesCb(u8 status, u8* result)
{
    enum { FILE_SYSTEM_HED_STREAM_HEADER_MASK = 0x7FFFFFFF };
    CdlLOC                 currLoc[3];
    s32                    currPos;
    u32                    headerOffset;
    u32                    streamIdx;
    u32                    fileId;
    u32                    fileCategory;
    u8                     isValidCategory;
    u32                    i;
    u32*                   entry;
    u8*                    entryBytes;
    u8*                    streamCpyPos;
    _FsStage0CategoryFile* tbl;
    FsSector*              sectorBuffer;
    // The cursor is reused for input words and the full-size output records.
    union {
        u32*       words;
        FsCdfFile* files;
    } cursor;
    StreamSlot* streamTable;
    u32*        fileSect90;
    u16*        fileSect5;
    u16*        fileSect0;

    streamIdx = 0;
    if (status != CdlDiskError) {
        // The first 3 words contain the sector header.
        // Make sure that we seeked to the correct location.
        Fs_VBlank = VSync(-1);
        CdGetSector(currLoc, 3);

        Fs_CurrSector = currPos = CdPosToInt(currLoc);
        if (currPos != Fs_ReqSector) {
            Fs_OnCdError(FS_ERROR_SOFT);
            return;
        }

        // Read the sector data.
        Fs_ReqSector += 1;
        CdGetSector(Fs_CdSector.words, FS_SECTOR_WORD_SIZE);

        headerOffset = 0;

        while (true) {
            sectorBuffer = &Fs_CdSector;
            streamTable  = Fs_Streams;
            fileSect0    = Fs_FileOffsetsCat0;
            fileSect5    = Fs_FileOffsetsCat5;
            fileSect90   = Fs_FileOffsetsCat90;

            if ((u16)headerOffset >= FS_SECTOR_WORD_SIZE) {
                return;
            }

            entry  = &sectorBuffer->words[(u16)headerOffset];
            fileId = *entry;
            if (fileId == FS_CDF_STAGE0_CANARY) {
                CdReadyCallback(NULL);
                Fs_CdOpStatus = -1;
                CdControlF(CdlPause, NULL);
                return;
            }

            if ((s32)fileId < 0) {
                *entry    &= FILE_SYSTEM_HED_STREAM_HEADER_MASK;
                entryBytes = (u8*)entry;

                // Copy the stream header into the stream table.
                streamCpyPos = (u8*)&streamTable[(u16)streamIdx];
                for (i = 0; (u16)i < sizeof(StreamSlot); i++) {
                    streamCpyPos[(u16)i] = entryBytes[(u16)i];
                }

                // Move to the next entry and adjust the offset to be the absolute
                // offset on the CD rom.
                streamTable[(u16)streamIdx++].startSector += Fs_StageCdfSectors[0];
                headerOffset                              += sizeof(StreamSlot) / sizeof(u32);
            } else {
                fileCategory = fileId / 10000;

                isValidCategory = false;
                switch (fileCategory) {
                    case 0:
                        i = 0;
                        while (true) {
                            fileSect0[(u16)i] = (&sectorBuffer->words[(u16)headerOffset])[1];
                            i++;
                            if ((u16)i >= 0x2D) {
                                break;
                            }
                            headerOffset += 2;
                        }
                        isValidCategory = true;
                        break;

                    case 1: {
                        u32 n;

                        isValidCategory = true;
                        tbl             = Fs_FileTableCat1;
                        n               = Fs_FileTableCat1Len;
                        Fs_FileTableCat1Len++;
                        tbl[n].idInCategory = fileId - 10000;
                        tbl[n].sectorOffset = entry[1];
                        break;
                    }

                    case 2: {
                        u32 n;

                        isValidCategory = true;
                        tbl             = Fs_FileTableCat2;
                        n               = Fs_FileTableCat2Len;
                        Fs_FileTableCat2Len++;
                        tbl[n].idInCategory = fileId - fileCategory * 10000;
                        tbl[n].sectorOffset = entry[1];
                        break;
                    }

                    case 3: {
                        u32 n;

                        isValidCategory = true;
                        tbl             = Fs_FileTableCat3;
                        n               = Fs_FileTableCat3Len;
                        Fs_FileTableCat3Len++;
                        tbl[n].idInCategory = fileId - 30000;
                        tbl[n].sectorOffset = entry[1];
                        break;
                    }

                    case 4: {
                        u32 n;

                        isValidCategory = true;
                        tbl             = Fs_FileTableCat4;
                        n               = Fs_FileTableCat4Len;
                        Fs_FileTableCat4Len++;
                        tbl[n].idInCategory = fileId - fileCategory * 10000;
                        tbl[n].sectorOffset = entry[1];
                        break;
                    }

                    case 5:
                        fileSect5[*entry % 100] = entry[1];
                        isValidCategory         = true;
                        break;

                    case 90:
                        fileSect90[*entry % 100] = entry[1];
                        isValidCategory          = true;
                        break;
                }

                if (!isValidCategory) {
                    u32 id;

                    cursor.words = sectorBuffer->words;
                    id           = cursor.words[(u16)headerOffset];
                    if (id / 100000 != 0) {
                        u32 n;

                        cursor.files = Fs_FileTable;
                        n            = Fs_FileTableLen;
                        Fs_FileTableLen++;
                        cursor.files[n].fileId       = id;
                        cursor.files[n].sectorOffset = (&sectorBuffer->words[(u16)headerOffset])[1];
                    }
                }

                headerOffset += sizeof(FsCdfFile) / sizeof(u32);
            }
        }
    }

    Fs_OnCdError(FS_ERROR_SOFT);
}

/* ISO directory name suffixes / special files (must sit in .rodata before ScanIso jtbl). */
static const char Fs_ExtensionCdf[]      = ".CDF";
static const char Fs_ExtensionStr[]      = ".STR";
static const char Fs_Stage0HeaderName[]  = "STAGE0.HED";
static const char Fs_InitBitstreamName[] = "INIT.BS";

void Fs_ScanIsoDirectory(s32 mode)
{
    u8* entry;
    s32 initBsSector;
    s32 initBsCount;
    s32 done;
    s32 i;
    s32 hasDot;
    u8  idx;
    s32 c;

    initBsSector = 0;
    initBsCount  = 0;

    SetMem(2);

restart:
    _fsStartRead(0x16, 0x16, Fs_CdSector.bytes, 0);
    while (Fs_CdOpStatus != 0xFF) {
        if (Fs_CdOpStatus == 0x80) {
            Fs_ClearDiskError();
            goto restart;
        }
        VSync(0);
    }

    {
        u8* sec = Fs_CdSector.bytes;

        if (sec[0] != 0x30 || sec[1] != 0) {
            _fsStartRead(0x14, 0x14, sec, 0);
            while (Fs_CdOpStatus != 0xFF) {
                if (Fs_CdOpStatus == 0x80) {
                    Fs_ClearDiskError();
                    goto restart;
                }
                VSync(0);
            }
        }
    }

    idx = 0;
    do {
        Fs_StageCdfSectors[idx] = 0;
        idx++;
    } while (idx < 6);

    entry                   = Fs_CdSector.bytes;
    done                    = 0;
    D_8006AC30.field_4      = 0;
    Wip_SysFlags.discNumber = GAME_MAIN_DISC_UNKNOWN;
    D_8006AC30.startSector  = 0;
    Fs_Stage0HedSector      = 0;

    while ((done & 0xFF) == 0) {
        switch (entry[0]) {
            case 0x38:
            case 0x3A:
            case 0x3C:
            case 0x3E:
                i      = 0;
                hasDot = 0;
                while (1) {
                    c = entry[0x21 + (i & 0xFF)];
                    if (c == 0x2E) {
                        hasDot = 1;
                        break;
                    }
                    if (c == 0) {
                        break;
                    }
                    i++;
                }
                if ((hasDot & 0xFF) != 0) {
                    s32 nameIdx = i & 0xFF;
                    u8* name    = &entry[0x21 + nameIdx];

                    if (strncmp(Fs_ExtensionCdf, (char*)name, 4) == 0) {
                        u8 stageNum;

                        stageNum = entry[0x20 + nameIdx] - 0x30;
                        Fs_StageCdfSectors[stageNum] =
                            *(u16*)(entry + 2) + (*(u16*)(entry + 4) << 16);
                    } else if (strncmp(Fs_ExtensionStr, (char*)name, 4) == 0) {
                        D_8006AC30.startSector =
                            *(u16*)(entry + 2) + (*(u16*)(entry + 4) << 16);
                    } else if (strncmp(Fs_Stage0HeaderName, (char*)(entry + 0x21), 0xA) == 0) {
                        Fs_Stage0HedSector =
                            *(u16*)(entry + 2) + (*(u16*)(entry + 4) << 16);
                    } else if (strncmp(Fs_InitBitstreamName, (char*)(entry + 0x21), 7) == 0) {
                        initBsCount = 0x10;
                        initBsSector =
                            *(u16*)(entry + 2) + (*(u16*)(entry + 4) << 16);
                    }
                }
            case 0x30:
            case 0x32:
            case 0x34:
            case 0x36:
                entry += entry[0];
                break;
            default:
                done = 1;
                break;
        }
    }

    if (Fs_StageCdfSectors[1] != 0 || Fs_StageCdfSectors[2] != 0) {
        Wip_SysFlags.discNumber = GAME_MAIN_DISC_1;
    }
    if (Fs_StageCdfSectors[4] != 0 || Fs_StageCdfSectors[5] != 0) {
        Wip_SysFlags.discNumber = GAME_MAIN_DISC_2;
    }

    Fs_ClearDiskError();

    if ((mode & 0xFF) != 0) {
        if (initBsSector != 0) {
            Fs_ChunkMode    = 0;
            D_8006ADF8      = 0;
            D5B498_8006EA1A = 0;
            D5B498_8006EBB0 = 0;
            D5B498_8006D850 = NULL;
            D5B498_8006D748 = 0;
            D5B498_8006D858 = 1;
            D_8006C4D4      = Fs_CdSector.bytes;
            D_8006C4D4     += FS_SECTOR_BYTE_SIZE;
            mdecRequestImageVlcRebuild();

            _fsStartRead(initBsSector, initBsSector + initBsCount, NULL, 5);
            while (Fs_CdOpStatus != 0xFF) {
                if (Fs_CdOpStatus == 0x80) {
                    if (CdSync(1, NULL) == CdlDiskError) {
                        Fs_WaitDiskReset(true);
                    }
                    goto restart;
                }
                VSync(0);
            }
            Fs_ClearDiskError();
        }
    }

    if (Fs_Stage0HedSector != 0) {
        CdlLOC loc[2];

        Fs_CdOpStatus       = 0;
        Fs_FileTableLen     = 0;
        Fs_FileTableCat2Len = 0;
        Fs_FileTableCat4Len = 0;
        Fs_FileTableCat1Len = 0;
        Fs_FileTableCat3Len = 0;
        Fs_ReqSector        = Fs_Stage0HedSector;
        CdIntToPos(Fs_Stage0HedSector, loc);
        CdControlF(CdlReadN, (u8*)loc);
        CdReadyCallback(Fs_InitStage0TablesCb);
        Fs_VBlank = VSync(-1);
    } else if ((mode & 0xFF) != 0) {
        goto restart;
    } else {
        Wip_SysFlags.discNumber = GAME_MAIN_DISC_UNKNOWN;
    }
}

u8 Fs_LoadImageChunk(FsImageChunk* chunk, u8 arg1)
{
    u_long*       ot;
    s32           retry;
    s8            yAdj;
    FsImageChunk* img;
    u32           inRange;

    if (ResetRCnt(RCntCNT2) == 0) {
        return 0xFF;
    }

    do {
        ot = BreakDraw();
        /* BreakDraw returns the SDK sentinel -1 when it cannot suspend DMA. */
        if (ot != FS_DRAW_BREAK_FAILED) {
            break;
        }
        if (GetRCnt(RCntCNT2) >= 0x6E40) {
            if (arg1 == 0) {
                Fs_ContinueDrawing(ot);
                return 0x7F;
            }
        }
    } while (1);

    D_8006C4C8[D5B498_8006ADF4] = 0;
    Fs_ImageRect.x              = chunk->x;

    inRange = (u32)(chunk->y - 0xF5) < 0xBU;
    img     = chunk;
    if (inRange) {
        yAdj = D5B498_8006C234;
    } else {
        yAdj = 0;
    }

    if (Fs_ChunkMode == 2) {
        Fs_ImageRect.y = yAdj + (img->y + 1);
    } else {
        Fs_ImageRect.y = img->y + yAdj;
    }

    Fs_ChunkReadPtr  = (u8*)(chunk + 1);
    Fs_ImageRect.w   = img->w;
    Fs_ChunkWritePtr = (u8*)D5B498_8006D870;
    Fs_ImageRect.h   = img->h;
    Fs_DecompressImage();

    if (D5B498_8006D748 == 0xFFFF) {
        Fs_ContinueDrawing(ot);
        return 0x7F;
    }

    LoadImage2(&Fs_ImageRect, D5B498_8006D870);

    retry = arg1;
    do {
        while (IsIdleGPU(-1) != 0) {
        }
        if (GetRCnt(RCntCNT2) < 0x6E40) {
            break;
        }
        if (retry != 0) {
            break;
        }
        ContinueDraw(NULL, ot);
        return 0x7F;
    } while (0);

    D_8006C4C8[D5B498_8006ADF4] = (u8)Fs_ImageRect.h;
    Fs_ContinueDrawing(ot);
    return 0;
}

void Fs_CopyWorkEntries(FsImageColumn* arg0)
{
    FsImageColumn* src;
    s32            i;

    src = arg0;
    i   = 0;
    while (1) {
        Fs_WorkEntries[i].x          = src->x;
        Fs_WorkEntries[i].y          = src->y;
        Fs_WorkEntries[i].dataOffset = src->dataOffset;
        if (Fs_WorkEntries[i].x == FILE_SYSTEM_IMAGE_COLUMN_END) {
            break;
        }
        src++;
        i++;
    }

    if ((Fs_WorkEntries[0].y >= 0x100U) || (Fs_ChunkMode == 2)) {
        Fs_ImageRect.x = Fs_WorkEntries[0].x + D5B498_8006C233 * 64;
    } else {
        Fs_ImageRect.x = Fs_WorkEntries[0].x;
    }

    if (Fs_ChunkMode == 2) {
        Fs_ImageRect.y = Fs_WorkEntries[0].y + 0x80;
    } else {
        Fs_ImageRect.y = Fs_WorkEntries[0].y;
    }

    Fs_ImageRect.w  = 0x40;
    Fs_ImageRect.h  = 0x20;
    D5B498_8006ACD4 = FILE_SYSTEM_IMAGE_COLUMN_ROWS;

    Fs_ChunkReadPtr = (u8*)arg0 + Fs_WorkEntries[0].dataOffset;

    // A terminator in the second record may set the single column's height.
    if (Fs_WorkEntries[1].x == FILE_SYSTEM_IMAGE_COLUMN_END) {
        if (Fs_WorkEntries[1].y == Fs_WorkEntries[1].x) {
            D5B498_8006ACD4 = FILE_SYSTEM_IMAGE_COLUMN_SHORT_ROWS;
        } else if (Fs_WorkEntries[1].y & FILE_SYSTEM_IMAGE_COLUMN_ROWS_GIVEN) {
            D5B498_8006ACD4 = Fs_WorkEntries[1].y & 0x7FFF;
        }
    }

    D5B498_8006ADE1                  = 1;
    D5B498_8006ADE0                  = 1;
    D5B498_8006D4E0[D5B498_8006ADF4] = 0;
}

u8 Fs_LoadImageStrip(s32 mode)
{
    u_long*        ot;
    u_long*        none;
    s32            retry;
    u8             count;
    u8*            scan;
    FsImageColumn* entry;
    RECT*          rect;

    if (ResetRCnt(RCntCNT2) == 0) {
        return 0xFF;
    }
    /* SDK status value, not a C object address; also accepted by ContinueDraw. */
    none  = FS_DRAW_BREAK_FAILED;
    retry = (u8)mode;
    for (;;) {
        ot = BreakDraw();
        if (ot != none) {
            break;
        }
        if (GetRCnt(RCntCNT2) >= 0x6E40) {
            if (retry == 0) {
                Fs_ContinueDrawing(FS_DRAW_BREAK_FAILED);
                return 0x7F;
            }
        }
    }

    for (;;) {
        if (D5B498_8006ADE1 != 0) {
            Fs_ChunkWritePtr = (u8*)D5B498_8006D870;
            D5B498_8006D748  = 0;
            D5B498_8006EA1A  = 0;
            D5B498_8006EBB0  = 0;
            D5B498_8006D850  = 0;
            D5B498_8006D85A  = 0;
            D5B498_8006D858  = 1;
            D5B498_8006ADE1  = 0;
        }
        Fs_DecompressChunk();
        if (D5B498_8006D748 == 0xFFFF) {
            Fs_ContinueDrawing(ot);
            return 0x7F;
        }
        if (D5B498_8006D748 == 0) {
            Fs_ContinueDrawing(ot);
            if ((u8)mode == 0) {
                Fs_ChunkReadPtr = Fs_CdSector.bytes;
                if (GetRCnt(RCntCNT2) >= 0x6E40) {
                    return 0x7F;
                }
            }
            return 0;
        }
        LoadImage2(&Fs_ImageRect, D5B498_8006D870);
        retry = (u8)mode;
        do {
            while (IsIdleGPU(-1) != 0) {
            }
            if (GetRCnt(RCntCNT2) < 0x6E40) {
                break;
            }
            if (retry != 0) {
                break;
            }
            ContinueDraw(0, ot);
            return 0x7F;
        } while (0);
        D5B498_8006ADE1  = 1;
        Fs_ImageRect.y  += 0x20;
        D5B498_8006ACD4 -= 0x20;
        if ((s16)D5B498_8006ACD4 <= 0) {
            if (Fs_WorkEntries[D5B498_8006ADE0].x == FILE_SYSTEM_IMAGE_COLUMN_END) {
                Fs_ContinueDrawing(ot);
                if ((u8)mode == 0) {
                    Fs_ChunkReadPtr = Fs_CdSector.bytes;
                    if (GetRCnt(RCntCNT2) >= 0x6E40) {
                        return 0x7F;
                    }
                }
                return 1;
            }
            D5B498_8006D4E0[D5B498_8006ADF4]++;
            entry = &Fs_WorkEntries[D5B498_8006ADE0];
            if (entry->y >= 0x100U || Fs_ChunkMode == 2) {
                Fs_ImageRect.x = entry->x + D5B498_8006C233 * 64;
            } else {
                Fs_ImageRect.x = entry->x;
            }
            D5B498_8006ACD4 = FILE_SYSTEM_IMAGE_COLUMN_ROWS;
            rect            = &Fs_ImageRect;
            rect->y         = Fs_WorkEntries[D5B498_8006ADE0].y;
            rect->w         = 0x40;
            rect->h         = 0x20;
            D5B498_8006ADE0++;
        }
        count = 0;
        scan  = Fs_ChunkReadPtr;
        do {
            {
                u8 value = *scan++;
                if (value != 0) {
                    goto strip_done;
                }
            }
            Fs_ChunkReadPtr++;
            count++;
            if (Fs_ChunkReadPtr >= D_8006CCD8 || count >= 6U) {
                Fs_ContinueDrawing(ot);
                if ((u8)mode == 0) {
                    Fs_ChunkReadPtr = D_8006CCD8 - 0x800;
                    if (GetRCnt(RCntCNT2) >= 0x6E40) {
                        return 0x7F;
                    }
                }
                return 0;
            }
        } while (1);
    strip_done:
        D5B498_8006D748 = 0;
    }
}

void Fs_ClearDiskError(void)
{
    u8  done;
    s32 status;
    u8  ctrlParam[8];
    u8  ctrlResult[8];

    done = 0;
    do {
        // Poll the status of the current command.
        status = CdSync(1, NULL);
        switch (status) {
            case CdlNoIntr:
                break;
            case CdlComplete:
                done = 1;
                break;
            case CdlDiskError:
                // Wait for the command to finish and reset the operation mode.
                CdSync(0, NULL);
                ctrlParam[0] = 0;
                CdControlB(CdlSetmode, ctrlParam, NULL);
                VSync(3);

                // Wait until the CD shell is closed with a valid disk.
                do {
                    do {
                        CdControlB(CdlNop, NULL, ctrlResult);
                    } while (ctrlResult[0] & CdlStatShellOpen);
                } while (CdDiskReady(0) != CdlComplete || CdGetDiskType() != CdlCdromFormat);

                // Enable double speed and sector header.
                ctrlParam[0] = CdlModeSpeed | CdlModeSize1;
                CdControlB(CdlSetmode, ctrlParam, NULL);
                VSync(3);
        }
    } while (done == 0);
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
        CdSyncCallback(Fs_ReadNSyncCb);
        Fs_VBlank      = VSync(-1);
        Fs_CdOpStatus += 1;
    }
}

u8 Fs_WaitDiskSwap(void)
{
    s32    status;
    u8     ctrlParam[8];
    u8     ctrlResult[8];
    CdlLOC loc[2];

    VSync(0);

    // Wait until the CD shell is opened.
    do {
        CdControlB(CdlNop, NULL, ctrlResult);
    } while ((ctrlResult[0] & CdlStatShellOpen) == 0);

    // Wait until the CD shell is closed.
    do {
        CdControlB(CdlNop, NULL, ctrlResult);
    } while ((ctrlResult[0] & CdlStatShellOpen) != 0);

    // Wait until the CD is spinning.
    while ((ctrlResult[0] & CdlStatStandby) == 0) {
        CdControlB(CdlNop, NULL, ctrlResult);
    }

    // Wait until the TOC is read.
    status = CdControlB(CdlGetTN, NULL, ctrlResult);
    while (ctrlResult[0] != CdlStatStandby || status == CdlNoIntr) {
        VSync(0x1e);
        status = CdControlB(CdlGetTN, NULL, ctrlResult);
    }

    // Enable double speed.
    ctrlParam[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, ctrlParam, NULL);
    VSync(3);

    // Read the volume descriptor (sector 16).
    CdIntToPos(0x10, loc);
    CdControlB(CdlReadN, &loc[0].minute, ctrlResult);

    // A disk error fails the swap. The status and error bytes are still
    // tested, but every outcome of those tests returns the same -1.
    if (CdSync(0, ctrlResult) == CdlDiskError) {
        if (ctrlResult[0] & CdlStatError) {
            if (ctrlResult[1] & 0x40) {
                return -1;
            }
        }
        return -1;
    }

    // Enable the sector header.
    CdControlB(CdlPause, NULL, ctrlResult);
    ctrlParam[0] = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, ctrlParam, NULL);
    return 0;
}

void Fs_ReadSectorEx(s32 sector, s32 arg1, u8* arg2, u8 arg3)
{
    _fsStartRead(sector, arg1, arg2, arg3);
}

static void Fs_ReadSector(s32 sector)
{
    CdlLOC loc[2];

    if (CdSync(1, NULL) == CdlDiskError) {
        Fs_WaitDiskReset(true);
    }

    Fs_CdOpStatus   = 0;
    Fs_ChunkEndFlag = -1;
    Fs_ReqSector    = sector;
    Fs_VBlank       = VSync(-1);
    if (Fs_SeekSector == sector) {
        Fs_SeekSector = 0;
        Fs_Streaming  = false;
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(Fs_CdReadyCb);
    } else {
        Fs_SeekSector = 0;
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(Fs_ReadNSyncCb);
    }
}

void Fs_WaitDiskReset(s8 withSectHdr)
{
    u8 ctrlParam[8];
    u8 ctrlResult[8];

    // Wait for the command to finish and reset the operation mode.
    CdSync(0, NULL);
    ctrlParam[0] = 0;
    CdControlB(CdlSetmode, ctrlParam, NULL);
    VSync(3);

    // Wait until the CD shell is opened and closed again with a valid disk.
    do {
        do {
            CdControlB(CdlNop, NULL, ctrlResult);
        } while ((ctrlResult[0] & CdlStatShellOpen) != 0);
    } while (CdDiskReady(0) != CdlComplete || CdGetDiskType() != CdlCdromFormat);

    // Enable double speed and optionally also the sector header.
    if (withSectHdr != 0) {
        ctrlParam[0] = CdlModeSpeed | CdlModeSize1;
    } else {
        ctrlParam[0] = CdlModeSpeed;
    }
    CdControlB(CdlSetmode, ctrlParam, NULL);
    VSync(3);
}

static void Fs_SeekToPos(s32 sector)
{
    CdlLOC loc[2];

    Fs_CdOpStatus = 0;
    if (CdSync(1, NULL) == CdlDiskError) {
        Fs_WaitDiskReset(true);
    }

    Fs_SeekSector = sector;
    CdIntToPos(sector, loc);
    CdControlF(CdlSeekL, (u8*)loc);
    CdSyncCallback(Fs_SeekToPosCb);
    Fs_VBlank = VSync(-1);
}

void Fs_InitFolderTable(s32 unused)
{
    u32                   offset;
    FsCdfFolderListEntry* entry;

    Fs_FolderTableLen = 0;
    entry             = Fs_CdSector.folderList.entries;

    // The folder table fills the CDF's first sector, so the first folder
    // starts at sector 1. Each sector count places the folder after it.
    offset = 1;
    while (true) {
        if (entry->sectorCount == FS_CDF_FOLDER_CANARY) {
            return;
        }

        Fs_FolderTable[Fs_FolderTableLen].folderId     = entry->folderId;
        Fs_FolderTable[Fs_FolderTableLen].sectorOffset = offset;
        Fs_FolderTableLen                             += 1;

        offset += entry->sectorCount;
        entry  += 1;
    }
}

void Fs_InitStage0Tables(void)
{
    CdlLOC headerPos;

    // Reset all tables.
    Fs_CdOpStatus       = 0;
    Fs_FileTableLen     = 0;
    Fs_FileTableCat2Len = 0;
    Fs_FileTableCat4Len = 0;
    Fs_FileTableCat1Len = 0;
    Fs_FileTableCat3Len = 0;

    // Read the stage header.
    Fs_ReqSector = Fs_Stage0HedSector;
    CdIntToPos(Fs_Stage0HedSector, &headerPos);
    CdControlF(CdlReadN, &headerPos.minute);
    CdReadyCallback(Fs_InitStage0TablesCb);
    Fs_VBlank = VSync(-1);
}

static void Fs_ReadNSyncCb(u8 status, u8* result)
{
    if (status != CdlDiskError) {
        if (Fs_CdOpStatus == 0x41) {
            Fs_CdOpStatus = 0;
            Fs_Streaming  = true;
            CdReadyCallback(Fs_CdReadyCb);
        } else if (D5B498_8006ACC8 == false) {
            Fs_Streaming = false;
            CdReadyCallback(Fs_CdReadyCb);
        } else {
            SndLoad_FromSectorMode8(&Fs_CdSector);
            CdReadyCallback(Fs_StreamReadyCb);
        }

        Fs_VBlank = VSync(-1);
        CdSyncCallback(NULL);
        Fs_CdErrorCount = 0;
    } else {
        Fs_OnCdError(FS_ERROR_SOFT);
    }
}

static void Fs_ReadNReadyCb(u8 status, u8* result)
{
    if (status != CdlDiskError) {
        Fs_VBlank       = VSync(-1);
        Fs_CdErrorCount = 0;
        Fs_Streaming    = 1;
        CdReadyCallback(Fs_CdReadyCb);
        CdSyncCallback(NULL);
    } else {
        Fs_OnCdError(FS_ERROR_SOFT);
    }
}

static void Fs_SeekToPosCb(u8 status, u8* result)
{
    if (status == CdlComplete) {
        Fs_CdOpStatus = FS_CD_STATUS_IDLE;
        CdSyncCallback(NULL);
        Fs_CdErrorCount = 0;
    } else {
        Fs_OnCdError(FS_ERROR_SOFT);
    }
}

static void Fs_OnCdError(u8 arg0)
{
    Fs_CdErrorCount += 1;
    CdReadyCallback(NULL);
    CdSyncCallback(NULL);

    if (arg0 == FS_ERROR_HARD) {
        Fs_CdOpStatus = 0x40;
    } else {
        SndLoad_Teardown();
        Fs_CdOpStatus = 0x80;
    }

    CdControlF(CdlPause, NULL);
}

static void Fs_ContinueDrawing(u_long* ot)
{
    // Wait until the gpu is idling and then continue drawing. With a NULL
    // first argument, ContinueDraw passes ot to the DMA address register,
    // including the -1 sentinel; it does not dereference ot as a C object.
    while (IsIdleGPU(-1) != 0) {
    }
    ContinueDraw(NULL, ot);
}

u8* Fs_GetChunkPayload(void)
{
    return Fs_CdSector.chunk.data.bytes;
}

void Fs_CheckReadTimeout(void)
{
    u8 ctrlResult[8];

    if (Fs_CdOpStatus != 0 && Fs_CdOpStatus != 0x41) {
        return;
    }

    if (VSync(-1) <= Fs_VBlank + 0xb4) {
        return;
    }

    Fs_VBlank     = VSync(-1);
    Fs_CdOpStatus = 0x80;
    CdFlush();
    SndLoad_Teardown();
    CdReadyCallback(NULL);
    CdSyncCallback(NULL);
    CdControlB(CdlPause, NULL, ctrlResult);
}

void Fs_StopCd(void)
{
    u8 ctrlParam[9];

    ctrlParam[0] = 0;
    ctrlParam[8] = 0;
    CdControlB(CdlSetmode, ctrlParam, NULL);

    VSync(3);
    CdControlB(CdlStop, NULL, NULL);
}

s32 Fs_GetStageDiskKind(void)
{
    u8 stage;

    stage = gGameSession->location.loc.stage;
    if (Fs_StageCdfSectors[stage] == 0) {
        if (stage == 1 || stage == 2) {
            return 1;
        }
        if (stage == 4 || stage == 5) {
            return 2;
        }
    }
    return 0;
}
