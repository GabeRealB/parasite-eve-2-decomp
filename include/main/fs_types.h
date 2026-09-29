#ifndef MAIN_FS_TYPES_H
#define MAIN_FS_TYPES_H

#include <psyq/sys/types.h>

#include "common.h"

#include "main/stream_types.h"

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

/// Per-slot decode entry at `CdCmdQueue.field_58` (5 entries, stride 0x3C).
/// Used by `Mdec_ResolveStreamBuffer` to map a stream id to a base buffer + byte offset.
/// Also the 0x3C-byte on-disc sector header read by the gameplay scene loader.
/// Holds up to 3 work-list / image-chunk offsets used by `Mdec_ProcessDecode`.
typedef struct _CdCmd58Entry {
    /* 0x00 */ s32  field_0;               // byte offset added to the resolved base buffer
    /* 0x04 */ s32  field_4[3];            // FsWorkEntry offsets (Fs_CopyWorkEntries path)
    /* 0x10 */ s32  field_10[3];           // FsImageChunk offsets (Fs_LoadImageChunk path)
    /* 0x1C */ s32  field_1C;              // memcpy source offset for Mem_CopyUnaligned
    /* 0x20 */ s32  nextDecodeBufferBytes; // Next allocation size, supplied by slot-zero sectors
    /* 0x24 */ s16  field_24[3];           // non-zero → Fs_ChunkMode=2 / D5B498_8006C233=-8
    /* 0x2A */ s16  field_2A[3];           // non-zero → Fs_ChunkMode=2 / D5B498_8006C234=-3
    /* 0x30 */ s16  sectorCount;           // Payload sectors after this header
    /* 0x32 */ s16  field_32;              // stream id matched against GameSession.at4.loc.view
    /* 0x34 */ s16  field_34;              // buffer-select kind (0..4) for the switch in Mdec_ResolveStreamBuffer
    /* 0x36 */ s16  field_36;              // Zero permits skipping a cached payload
    /* 0x38 */ u16  field_38;              // memcpy byte count for Mem_CopyUnaligned
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
    CdCmdEntry   field_50;              // replace-slot used by CdCmd_EnqueueReplace (cmd at 0x54)
    CdCmd58Entry field_58[5];           // 0x58..0x183 — stream decode slot table
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
    u16          field_216; // 0x216 — non-zero enables buffer setup in CdCmd_SetupMdecBuffers
    u16          field_218; // 0x218 — non-zero blocks Mdec_ResolveStreamBuffer success path
    s16          field_21A;
    u16          field_21C; // image transfer mode for Display_LoadImageStrips (0 / 1)
    u16          field_21E; // 0x21E — DecDCTvlcBuild done flag
    byte         unknown_220[0x2];
    u16          field_222;
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
    s16          field_244;
    u16          field_246;
    u16          field_248;
    u16          field_24A;
    u16          field_24C; // 0x24C
    u16          field_24E; // 0x24E
    byte         unknown_250[0x2];
    s16          busy;      // 0x252 — non-zero while a blocking load is active
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
