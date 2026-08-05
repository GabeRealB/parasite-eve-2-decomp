#ifndef MC_H
#define MC_H

#include "common.h"

#include "main/task.h"

// =============================================================================
// Types — memcard / save (src/main/mc.c, src/main/mcmenu.c)
// =============================================================================

/// Pair of prompt / dialog data pointers. Mc_PromptTable is an array of these,
/// indexed by mode (see Mc_DrawPrompt).
typedef struct _McPromptPair {
    /* 0x0 */ u8* field_0;
    /* 0x4 */ u8* field_4;
} McPromptPair;
STATIC_ASSERT_SIZEOF(McPromptPair, 0x8);

/// Second argument to memcard/save state handlers in mc.c (e.g. func_80035AD4,
/// Mc_ResetWork, func_80035FD8). Larger object; only fields used so far are named.
/// field_10/field_14 are MemCardSync cmds/rslt outs.
/// field_18 is a source buffer pointer for Mc_WriteDataChecksum when mode != 0
/// and the adrs for MemCardWriteData. field_1C is the sector/offset (shifted
/// left by 7 for MemCardWriteData ofs); field_20 is the byte count.
/// field_30 is a 15-slot memcard directory buffer (DIRENTRY-sized, 0x28 each)
/// filled by MemCardGetDirentry; field_288 is the entry count used to bound
/// field_A14 walks (Mc_StateOpenNext / func_800367CC). field_290 is cleared
/// when a non-empty directory listing is obtained (func_80036488). field_294
/// is a 15-slot array of 0x80-byte read buffers indexed by field_A14
/// (MemCardReadData adrs in func_800366BC, ofs 0x200, size 0x80). field_A14
/// indexes the selected slot for MemCardOpen / field_294 reads.
/// field_A1C/field_A1E are a sum / ones-complement checksum pair over 0x200
/// signed bytes of that buffer (written by Mc_WriteDataChecksum).
typedef struct _McWork {
    /* 0x000 */ s32           field_0;
    /* 0x004 */ s32           field_4;
    /* 0x008 */ s32           field_8;
    /* 0x00C */ s32           field_C;
    /* 0x010 */ s32           field_10;
    /* 0x014 */ s32           field_14;
    /* 0x018 */ s32           field_18;
    /* 0x01C */ s32           field_1C;
    /* 0x020 */ s32           field_20;
    /* 0x024 */ s32           field_24;
    /* 0x028 */ s32           field_28;
    /* 0x02C */ s32           field_2C;
    /* 0x030 */ char          field_30[15][0x28];
    /* 0x288 */ s32           field_288;
    /* 0x28C */ byte          unknown_28C[0x4];
    /* 0x290 */ s32           field_290;
    /* 0x294 */ unsigned long field_294[15][0x20];
    /* 0xA14 */ s32           field_A14;
    /* 0xA18 */ s32           field_A18;
    /* 0xA1C */ u16           field_A1C;
    /* 0xA1E */ u16           field_A1E;
    /* 0xA20 */ s32           field_A20;
} McWork;

/// BSS object Mc_SaveData. Large; only fields used so far are named.
/// field_12 is a slot/index validated by Mc_VerifySaveHdrChecksum (must be 1..16).
/// field_1C / field_1E are a sum / ones-complement pair over the 0x38 bytes
/// starting at field_4 (written by Mc_WriteSaveHdrChecksum, verified by
/// Mc_VerifySaveHdrChecksum). field_93C is a save-data checksum halfword
/// compared by Mc_CompareSaveChecksum. field_940 / field_942 are a sum /
/// ones-complement pair over the first byte of Mc_BufferSlots[1..8] buffers
/// (written by func_80033D3C; field_940 is also known as D_80072AA8).
typedef struct _McSaveData {
    /* 0x000 */ byte unknown_0[0x4];
    /* 0x004 */ u8   field_4;
    /* 0x005 */ u8   field_5;
    /* 0x006 */ u8   field_6;
    /* 0x007 */ u8   field_7;
    /* 0x008 */ u8   field_8;
    /* 0x009 */ u8   field_9;
    /* 0x00A */ byte unknown_A[0x8];
    /* 0x012 */ u8   field_12;
    /* 0x013 */ byte unknown_13[0x9];
    /* 0x01C */ u16  field_1C;
    /* 0x01E */ u16  field_1E;
    /* 0x020 */ byte unknown_20[0x1];
    /* 0x021 */ u8   field_21;
    /* 0x022 */ s8   field_22;
    /* 0x023 */ s8   field_23;
    /* 0x024 */ byte unknown_24[0x1];
    /* 0x025 */ u8   field_25;
    /* 0x026 */ byte unknown_26[0x182];
    /* 0x1A8 */ u8   field_1a8;
    /* 0x1A9 */ u8   field_1a9;
    /* 0x1AA */ u8   field_1aa;
    /* 0x1AB */ u8   field_1ab;
    /* 0x1AC */ byte unknown_1AC[0x416];
    /* 0x5C2 */ s8   field_5C2;
    /* 0x5C3 */ byte unknown_5C3[0x2];
    /* 0x5C5 */ u8   field_5C5;
    /* 0x5C6 */ byte unknown_5C6[0x376];
    /* 0x93C */ u16  field_93C;
    /* 0x93E */ byte unknown_93E[0x2];
    /* 0x940 */ s16  field_940;
    /* 0x942 */ s16  field_942;
} McSaveData;

/// Checksummed buffer header (sum / ones-complement at 0x0 / 0x2, payload at 0x4).
typedef struct _McChecksumBlock {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ u8  field_4[1];
} McChecksumBlock;

/// 0xC descriptor for a memcard/save buffer slot in Mc_BufferSlots[9].
/// field_0 points at a McChecksumBlock-style checksummed buffer; field_4 is its size.
/// Iterated from index 1..8 by func_80033D88 and related helpers in mc.c.
typedef struct _McBufferSlot {
    /* 0x0 */ McChecksumBlock* field_0;
    /* 0x4 */ s32              field_4;
    /* 0x8 */ s32              field_8;
} McBufferSlot;
STATIC_ASSERT_SIZEOF(McBufferSlot, 0xC);

/// Memcard state-machine handler: (Task*, McWork*).
typedef void (*McStateFunc)(Task* task, McWork* work);

/// Fixed-size table of McStateFunc callbacks. Copied onto the stack by
/// func_800359A4 so the call uses a local jump table (44 entries, 0xB0 bytes).
typedef struct {
    McStateFunc funcs[44];
} McStateFuncTable44;
STATIC_ASSERT_SIZEOF(McStateFuncTable44, 0xB0);

/// Fixed-size table of McStateFunc callbacks. Copied onto the stack by
/// func_80036968 so the call uses a local jump table (26 entries, 0x68 bytes).
typedef struct {
    McStateFunc funcs[26];
} McStateFuncTable26;
STATIC_ASSERT_SIZEOF(McStateFuncTable26, 0x68);

// =============================================================================
// Functions — src/main/mc.c (matched helpers; state handlers also live here)
// =============================================================================

void func_800319E4(Task* task, McWork* work);
void Mc_StateCreateFile(Task* task, McWork* work);
void Mc_StateFormat(Task* task, McWork* work);
void func_800327A4(Task* task, McWork* work);
void func_800328FC(Task* task, McWork* work);
void func_80034B38(Task* task, McWork* work);
void func_800359A4(Task* task);
u16* Mc_EncodeAsciiGlyphs(s8* src, u16* dst);
void Mc_InitFileName(void);
void Mc_CopyFileName(s32 direction);
void Mc_WriteSaveHdrChecksum(void);
s32  Mc_VerifySaveHdrChecksum(McSaveData* save);
void Mc_WriteBlockChecksum(McChecksumBlock* block, s32 size);
void Mc_ResetSaveFlags(void);
void Mc_ClearWorkBuffers(void);
void Mc_InitLib(void);
s32  Mc_VerifyBlockChecksum(McChecksumBlock* block, s32 size);
void Mc_DrawPrompt(Task* task, s32 mode);
void Mc_HideChildUi(Task* task);
void Mc_WriteDataChecksum(s32 mode, McWork* work);
s32  Mc_CompareSaveChecksum(McSaveData* save, McWork* work);
void Mc_ResetWork(Task* task, McWork* work);
void Mc_StateOpenSelected(Task* task, McWork* work);
void Mc_StateOpenNext(Task* task, McWork* work);
void Mc_StateCloseReturn(Task* task, McWork* work);
void func_80036968(Task* task);

// =============================================================================
// Globals
// =============================================================================

extern u8           Mc_FileName[0x18];
extern u8           Mc_FileNameBuf[0x18];
extern u16          Mc_GlyphsUpper[];
extern u16          Mc_GlyphsLower[];
extern u16          Mc_GlyphsSymbol[];
extern McBufferSlot Mc_BufferSlots[9];
extern u8           Mc_DefaultChecksumSrc[];
extern McPromptPair Mc_PromptTable[];
extern McSaveData   Mc_SaveData;
/// "Memory Card" string passed to func_80048E38 by Mc_DrawPrompt.
extern char D_8001398C[];
/// Jump table of 44 McStateFunc handlers used by func_800359A4.
extern McStateFuncTable44 D_800139AC;
/// Jump table of 26 McStateFunc handlers used by func_80036968.
extern McStateFuncTable26 D_80013ACC;
/// Global McWork instance used by the memcard state dispatcher.
extern McWork D_80071730;
/// Stores the result of rand() after each dispatcher tick.
extern s32 D_80073C08;

#endif // MC_H
