#ifndef MAIN_FS_TYPES_H
#define MAIN_FS_TYPES_H

#include <psyq/sys/types.h>

#include "common.h"

#include "main/stream_types.h"

/// Request opcodes stored in `CdCmdEntry.cmd`; the high nibble selects a handler.
enum {
    CD_COMMAND_EMPTY                     = 0,
    CD_COMMAND_LOAD_FILE                 = 0x21,
    CD_COMMAND_MOUNT_STAGE               = 0x54,
    CD_COMMAND_READ_STAGE_HEADER         = 0x55,
    CD_COMMAND_CONTINUE_STREAM           = 0x62,
    CD_COMMAND_PLAY_STREAM_AT_OFFSET     = 0x71,
    CD_COMMAND_RESET_STREAM_AT_OFFSET    = 0x72,
    CD_COMMAND_RESUME_STREAM_AT_POSITION = 0x73,
    CD_COMMAND_PLAY_SCENE_AUDIO          = 0x81,
    CD_COMMAND_START_SCENE_AUDIO         = 0x82,
};

/// File-load policies stored in `CdCmdEntry.args.file.loadMode`.
enum {
    CD_COMMAND_LOAD_DEFAULT         = 0,
    CD_COMMAND_LOAD_SEEK_ONLY       = 1,
    CD_COMMAND_LOAD_INIT_SOUND      = 2,
    CD_COMMAND_LOAD_RELOCATE_IMAGES = 3,
    CD_COMMAND_LOAD_SKIP_BACKGROUND = 4,
    CD_COMMAND_LOAD_IMAGES_ONLY     = 5,
    CD_COMMAND_LOAD_SKIP_SOUND      = 6,
};

/// An eight-byte CD request, used in the ring and its active/replacement slots.
///
/// `args` preserves all four bytes of the enqueue API's second parameter block.
/// File commands interpret them as an ID component and load options; stream
/// commands interpret them as a slot and, for 0x71/0x72, a signed 16-bit sector
/// offset packed high byte first. The other bytes are copied without interpreting
/// them when a request is saved or requeued.
///
/// Stage-zero file IDs are `fileGroup*10000 + args.file.fileIdHundreds*100 +
/// fileIndex`. Stages 1..5 use folder `fileGroup*100 + args.file.fileIdHundreds`
/// and `fileIndex` within it. `stage` also selects the CDF for mount commands.
/// A zero `cmd` marks an empty or retired record. Iterator pointers borrow ring
/// storage and must be copied before queue operations can overwrite the slot.
typedef struct {
    union {
        u8 bytes[4];             // Exact second parameter block, including bytes unused by an opcode
        struct {
            u8 fileIdHundreds;   // Stage-zero hundreds component; otherwise the folder-number suffix
            s8 loadMode;         // Load policy (0 normal, 1 seek, 2 sound setup, 3 relocate images, 4 skip background, 5 images only, 6 skip sound)
            s8 imageXPageOffset; // Signed strip X shift in 64-word VRAM pages (y >= 256 or load mode 3)
            s8 imageYOffset;     // Signed vertical image shift in VRAM rows, applied to image headers with y=245..255
        } file;
        struct {
            s8 slotIndex;        // Stream-slot index (0..14); negative values are invalid
            u8 sectorOffsetHigh; // High byte of the signed sector offset for 0x71/0x72
            u8 sectorOffsetLow;  // Low byte of that offset; the fourth argument byte is unused by stream handlers
        } stream;
    } args;
    u8 cmd;       // Request opcode (0 empty); high nibble identifies the command family
    u8 stage;     // CDF index (0 global library, 1..5 room folders); mount selector for 0x54
    u8 fileGroup; // Stage-zero file category; otherwise the folder-number hundreds component
    u8 fileIndex; // Stage-zero file-ID low component; otherwise the file index within the folder
} CdCmdEntry;
STATIC_ASSERT_SIZEOF(CdCmdEntry, 0x8);

/// Layout of the decoded room image consumed by the display uploader.
enum {
    FILE_SYSTEM_IMAGE_CONTIGUOUS = 0,
    FILE_SYSTEM_IMAGE_STRIPS     = 1,
};

/// Global CD / asset load command queue (`CdCmd_Queue`).
typedef struct _CdCmdQueue {
    // clang-format off
    CdCmdEntry   entries[8];
    CdCmdEntry   field_40;
    s32          field_48; // last CD position from CdPosToInt
    s8           field_4c;
    byte         unknown_4d[0x3];
    CdCmdEntry   field_50;              // replace-slot used by CdCmd_EnqueueReplace (cmd at 0x54)
    StreamSceneImageHeader field_58[5];           // 0x58..0x183 — stream decode slot table
    void*        decodeBuffer;          // 0x184 — aux buffer (malloc of decodeBufferBytes)
    s32          decodeBufferBytes;     // 0x188 — size for decodeBuffer malloc
    u16*         field_18C;             // 0x18C — VLC / DCT table buffer
    StreamSlot*  field_190;             // 0x190
    s32          nextDecodeBufferBytes; // 0x194 — byte count for the next decode-buffer allocation
    void*        field_198;             // 0x198 — base buffer for field_58 kind 4
    u32*         field_19C;             // 0x19C — copy of field_1A4; timing table cursor
    u32          field_1A0;             // 0x1A0 — timing accumulator (GameMain_Loop)
    void*        field_1A4;             // 0x1A4 — secondary image/stream buffer
    s32          field_1A8;             // 0x1A8 — copied to Gp_LcgState by Gp_RestoreStreamRng
    u32          field_1AC;             // 0x1AC — srand seed restored by Gp_RestoreStreamRng
    byte         unknown_1B0[0x18];
    u16          writeIdx;              // 0x1C8 — next free slot (enqueue)
    u16          readIdx;               // 0x1CA — slot being executed
    byte         unknown_1cc[0x4];
    u16          step;                  // 0x1D0 — sub-state of current command
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
    u16          field_216;      // 0x216 — non-zero enables buffer setup in CdCmd_SetupMdecBuffers
    u16          field_218;      // 0x218 — non-zero blocks Mdec_ResolveStreamBuffer success path
    s16          field_21A;
    u16          imageLayout;    // Room image layout (0 contiguous 320x240, 1 twenty 16x240 strips)
    u16          field_21E;      // 0x21E — DecDCTvlcBuild done flag
    byte         unknown_220[0x2];
    u16          pausePlayClock; // Nonzero pauses the play clock during CD file loads
    u16          field_224;
    u16          field_226;      // sub-state for CdCmd_RecoverDisk disk recovery
    u16          field_228;
    u16          field_22A;      // DecDCTin mode for Mdec_DecodeToVram
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
    s16          field_244;
    u16          field_246;
    u16          field_248;
    u16          field_24A;
    u16          field_24C; // 0x24C
    u16          field_24E; // 0x24E
    byte         unknown_250[0x2];
    s16          busy;      // 0x252 — non-zero while a blocking load is active
    // clang-format on
} CdCmdQueue;
STATIC_ASSERT_SIZEOF(CdCmdQueue, 0x254);

/// Per-folder slot cleared by `Fs_PrepareFolderLoad` (50 entries, parallel to
/// `Fs_FolderTable`). Only the first byte is written by the init path.
typedef struct _FsFolderSlot {
    u8    field_0;
    u8    pad_1[3];
    void* field_4; // The folder's loaded data; its type depends on the file
} FsFolderSlot;
STATIC_ASSERT_SIZEOF(FsFolderSlot, 0x8);

/// Image workspace, used as twenty 1920-word strips or one continuous frame.
/// A flat array also keeps whole-frame pixel processing within one C object.
typedef struct _FsImgBuffers {
    u_long words[20 * 1920];
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

#endif // MAIN_FS_TYPES_H
