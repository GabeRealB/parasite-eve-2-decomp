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

/// Second argument to memcard/save state handlers in mc.c (e.g. Mc_StateSetOpenDefaults,
/// Mc_ResetWork, Mc_StateSyncPrompt3). Larger object; only fields used so far are named.
/// field_10/field_14 are MemCardSync cmds/rslt outs.
/// field_18 is a source buffer pointer for Mc_WriteDataChecksum when mode != 0
/// and the adrs for MemCardWriteData. field_1C is the sector/offset (shifted
/// left by 7 for MemCardWriteData ofs); field_20 is the byte count.
/// field_30 is a 15-slot memcard directory buffer (DIRENTRY-sized, 0x28 each)
/// filled by MemCardGetDirentry; field_288 is the entry count used to bound
/// field_A14 walks (Mc_StateOpenNext / Mc_StateWalkDirectory). field_28C is free-block
/// count (updated as field_28C - field_288 after a directory listing in
/// Mc_StateListDirectory). field_290 is the Mc_FileName match index (or 0); cleared when
/// a non-empty directory listing is obtained (Mc_StateGetDirentry). field_294 is a
/// 15-slot array of 0x80-byte read buffers indexed by field_A14
/// (MemCardReadData adrs in Mc_StateReadSlot, ofs 0x200, size 0x80). field_A14
/// indexes the selected slot for MemCardOpen / field_294 reads.
/// field_A1C/field_A1E are a sum / ones-complement checksum pair over 0x200
/// signed bytes of that buffer (written by Mc_WriteDataChecksum).
/// Bytes at 0xA23+ form a block→direntry map written by Mc_StateListDirectory
/// (index = DIRENTRY.head / 64); field_A20 is a separate word flag (Mc_ResetWork
/// / Mc_StateInitWorkDefaults) whose high byte is the first map slot.
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
    /* 0x28C */ s32           field_28C;
    /* 0x290 */ s32           field_290;
    /* 0x294 */ unsigned long field_294[15][0x20];
    /* 0xA14 */ s32           field_A14;
    /* 0xA18 */ s32           field_A18;
    /* 0xA1C */ u16           field_A1C;
    /* 0xA1E */ u16           field_A1E;
    /* 0xA20 */ s32           field_A20;
    /* 0xA24 */ u8            field_A24[0x10];
} McWork;

/// 4-byte save-inventory row (`Mc_SaveData.field_1AC`).
typedef struct _McItemRec {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} McItemRec;
STATIC_ASSERT_SIZEOF(McItemRec, 0x4);

/// 8-byte save-inventory slot (`Mc_SaveData.field_1C8` / `field_5C8`).
/// field_0/field_2 are item ids; field_1/field_3 are the matching counts
/// (`Gp_CountEquippedRelated`). field_4 is word-cleared by `Gp_ResetAuxSlots`.
typedef struct _McItemSlot {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ s32 field_4;
} McItemSlot;
STATIC_ASSERT_SIZEOF(McItemSlot, 0x8);

/// 4-byte item-table scan descriptor (`Mc_SaveData.field_5BC`).
typedef struct _McItemScan {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} McItemScan;
STATIC_ASSERT_SIZEOF(McItemScan, 0x4);

/// 0xC-byte saved object pose. 32 of these occupy `Mc_SaveData.field_28`
/// (`Gp_SaveEnemyPose`). `field_3 == 0` is empty; a non-zero value is copied
/// from `GpEnemy.field_4B`. `field_A` is the object id (`GpEnemy.field_8`).
/// `field_4` / `field_6` / `field_8` are signed world XYZ (low 16 bits of
/// `GsCOORDINATE2.coord.t`). `field_0` / `field_1` / `field_2` are packed
/// euler bytes (`Gfx_MatrixToEuler` result `>> 8`).
typedef struct _McPosRec {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ s8  field_3;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u16 field_A;
} McPosRec;
STATIC_ASSERT_SIZEOF(McPosRec, 0xC);

/// BSS object Mc_SaveData. Large; only fields used so far are named.
/// field_12 is a slot/index validated by Mc_VerifySaveHdrChecksum (must be 1..16).
/// field_1C / field_1E are a sum / ones-complement pair over the 0x38 bytes
/// starting at field_4 (written by Mc_WriteSaveHdrChecksum, verified by
/// Mc_VerifySaveHdrChecksum). field_93C is a save-data checksum halfword
/// compared by Mc_CompareSaveChecksum. field_940 / field_942 are a sum /
/// ones-complement pair over the first byte of Mc_BufferSlots[1..8] buffers
/// (written by Mc_WriteFirstByteChecksum).
typedef struct _McSaveData {
    /* 0x000 */ byte       unknown_0[0x4];
    /* 0x004 */ u8         field_4;
    /* 0x005 */ u8         field_5;
    /* 0x006 */ u8         field_6;
    /* 0x007 */ u8         field_7;
    /* 0x008 */ u8         field_8;
    /* 0x009 */ u8         field_9;
    /* 0x00A */ byte       unknown_A[0x2];
    /* 0x00C */ u16        field_C;
    /* 0x00E */ s8         field_E;  // *2/5 scale for Gp_IdParamHi[i].field[0] (func_800D50D4)
    /* 0x00F */ s8         field_F;  // *4/5 scale for Gp_IdParamHi[i].field[0] (func_800D50D4)
    /* 0x010 */ u8         field_10; // init bitmask; bit 0 = global init (Gp_InitStageVisit)
    /* 0x011 */ byte       unknown_11;
    /* 0x012 */ u8         field_12;
    /* 0x013 */ s8         field_13; // 1-based index into Gp_AllyIdBase
    /* 0x014 */ s32        field_14; // Wip_SysConfig.field_8 (`Gp_SavePlayerPos`)
    /* 0x018 */ s32        field_18; // Wip_SysConfig.field_C (`Gp_SavePlayerPos`)
    /* 0x01C */ u16        field_1C;
    /* 0x01E */ u16        field_1E;
    /* 0x020 */ byte       unknown_20[0x1];
    /* 0x021 */ s8         field_21; // lb in title restore / gameflow
    /* 0x022 */ s8         field_22; // 1-based row for D_80112E2C
    /* 0x023 */ s8         field_23;
    /* 0x024 */ byte       unknown_24[0x1];
    /* 0x025 */ u8         field_25;
    /* 0x026 */ u8         field_26; // unsigned addend for Wip_SysConfig.field_1a (Gp_RecalcMaxHp); +5 in Gp_UiBoostHp
    /* 0x027 */ u8         field_27; // unsigned addend for Wip_SysConfig.field_1e (Gp_RecalcMaxMp); +1 in Gp_UiBoostMp
    /* 0x028 */ McPosRec   field_28[0x20];
    /* 0x1A8 */ s8         field_1a8;
    /* 0x1A9 */ s8         field_1a9;
    /* 0x1AA */ u8         field_1aa;
    /* 0x1AB */ s8         field_1ab;
    /* 0x1AC */ McItemRec  field_1AC[7];
    /* 0x1C8 */ McItemSlot field_1C8[0x7C];
    /* 0x5A8 */ byte       unknown_5A8[4];
    /* 0x5AC */ s32        field_5AC[4];
    /* 0x5BC */ McItemScan field_5BC;
    /* 0x5C0 */ byte       unknown_5C0[2];
    /* 0x5C2 */ s8         field_5C2;
    /* 0x5C3 */ s8         field_5C3;
    /* 0x5C4 */ byte       unknown_5C4;
    /* 0x5C5 */ s8         field_5C5;
    /* 0x5C6 */ byte       unknown_5C6[0x1];
    /* 0x5C7 */ s8         field_5C7;       // addend for Gp_AllyIdBase lookup
    /* 0x5C8 */ McItemSlot field_5C8[0x20]; // inited by Gp_ResetAuxSlots; index 0x1A gets field_2=0
    /* 0x6C8 */ u16        field_6C8;
    /* 0x6CA */ u16        field_6CA;
    /* 0x6CC */ u16        field_6CC;         // capped at 9999; incremented by Gp_AreaEnterTask on spawnArg1 == 0
    /* 0x6CE */ u16        field_6CE;         // capped at 9999; incremented by Gp_AreaEnterTask on spawnArg1 != 0
    /* 0x6D0 */ s32        field_6D0[0x60];   // bit flags; Gp_SetItemSeenBit sets/clears bit id for id < 0x180; Gp_HasItemSeenBit tests; Gp_InitItemSeenBits clears all 96 words
    /* 0x850 */ u8         unknown_850[0x12]; // 3-byte rows; Gp_DrawPeSlotRow indexes [spawnArg1*3 + slot]
    /* 0x862 */ s16        field_862[7];      // per-category use counters capped at 9999; Gp_UseItemTask indexes by Gp_StateC08.field_5, which can run past the 7 entries into field_870 and unknown_872
    /* 0x870 */ u16        field_870;         // capped at 9999; incremented by Gp_UseHealItemPanel
    /* 0x872 */ byte       unknown_872[0x16];
    /* 0x888 */ s32        field_888[0x20];   // 1-based counters; cap 0x1869E (func_80106518)
    /* 0x908 */ s8         field_908[0x20];   // signed addend for item ids 0x60–0x7F (Gp_GetModLevel)
    /* 0x928 */ byte       unknown_928[0x1];
    /* 0x929 */ s8         field_929;
    /* 0x92A */ byte       unknown_92A[0x12];
    /* 0x93C */ u16        field_93C;
    /* 0x93E */ byte       unknown_93E[0x2];
    /* 0x940 */ u16        field_940;
    /* 0x942 */ s16        field_942;
} McSaveData;
STATIC_ASSERT_SIZEOF(McSaveData, 0x944);

/// Checksummed buffer header (sum / ones-complement at 0x0 / 0x2, payload at 0x4).
typedef struct _McChecksumBlock {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ u8  field_4[1];
} McChecksumBlock;

/// 0xC descriptor for a memcard/save buffer slot in Mc_BufferSlots[9].
/// field_0 points at a McChecksumBlock-style checksummed buffer; field_4 is its size.
/// Iterated from index 1..8 by Mc_VerifyFirstByteChecksum and related helpers in mc.c.
typedef struct _McBufferSlot {
    /* 0x0 */ McChecksumBlock* field_0;
    /* 0x4 */ s32              field_4;
    /* 0x8 */ s32              field_8;
} McBufferSlot;
STATIC_ASSERT_SIZEOF(McBufferSlot, 0xC);

/// Memcard state-machine handler: (Task*, McWork*).
typedef void (*McStateFunc)(Task* task, McWork* work);

/// Fixed-size table of McStateFunc callbacks. Copied onto the stack by
/// Mc_DispatchStateTable so the call uses a local jump table (44 entries, 0xB0 bytes).
typedef struct {
    McStateFunc funcs[44];
} McStateFuncTable44;
STATIC_ASSERT_SIZEOF(McStateFuncTable44, 0xB0);

/// Fixed-size table of McStateFunc callbacks. Copied onto the stack by
/// Mc_DispatchStateTable26 so the call uses a local jump table (26 entries, 0x68 bytes).
typedef struct {
    McStateFunc funcs[26];
} McStateFuncTable26;
STATIC_ASSERT_SIZEOF(McStateFuncTable26, 0x68);

// =============================================================================
// Functions — src/main/mc.c (matched helpers; state handlers also live here)
// =============================================================================

void Mc_StateCompareBuffers(Task* task, McWork* work);
void Mc_StateOpenRead(Task* task, McWork* work);
void Mc_StateCreateFile(Task* task, McWork* work);
void Mc_StatePadFileName(Task* task, McWork* work);
void Mc_StateNameEntry(Task* task, McWork* work);
void Mc_StateBackupBuffers(Task* task, McWork* work);
void Mc_StateFreeBuffer(Task* task, McWork* work);
void Mc_StateFormat(Task* task, McWork* work);
void Mc_StateSyncFileSelect(Task* task, McWork* work);
void Mc_StateBlankFileName(Task* task, McWork* work);
void Mc_StateSyncOpen(Task* task, McWork* work);
void Mc_StateVerifyFinish(Task* task, McWork* work);
void Mc_StateFinishWrite(Task* task, McWork* work);
void Mc_KillIfCountdown(Task* task, McWork* work);
void Mc_DispatchStateTable(Task* task);
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
void Mc_DispatchStateTable26(Task* task);

/// Init Mc_BufferSlots[1..8] dual-bank buffers and related save state.
void Mc_InitBufferSlots(void);
/// Build a memcard save filename into `buf` (product code + mode char + random).
void Mc_BuildFileName(u8* buf, s32 modeCharIdx);
/// Defaults for the Mc_InitSaveSlotDefaults / Wip_SysConfig block (wipsyscfg).
void Mc_InitSaveSlotDefaults(void);

/// Prompt + optional choice dialog (Mc_PromptTable[mode]).
s32 Mc_PromptDialog(Task* task, s32 mode, s32 arg2);
s32 Mc_PromptDialogChoice(Task* task, s32 mode, s32 arg2);
s32 Mc_PromptDialogSpawn(Task* task, s32 mode, s32 arg2);
s32 Mc_PromptDialogFile(Task* task, s32 mode, s32 arg2);

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
/// "Memory Card" string passed to Ui_DrawTitle by Mc_DrawPrompt.
extern char D_8001398C[];
/// "*" wildcard passed to MemCardGetDirentry by Mc_StateScanDirFlags.
extern char D_80013A5C[];
/// Jump table of 44 McStateFunc handlers used by Mc_DispatchStateTable.
extern McStateFuncTable44 D_800139AC;
/// Jump table of 26 McStateFunc handlers used by Mc_DispatchStateTable26.
extern McStateFuncTable26 D_80013ACC;
/// Global McWork instance used by the memcard state dispatcher.
extern McWork D_80071730;
/// Memcard product-code prefix (12 bytes, e.g. "BASLUS-01042").
extern u8 D_80060DC8[];
/// 64-byte character table for random memcard filename body.
extern u8   D_80060E08[];
extern u8   D_80060A54[];
extern u8   D_80060A58[];
extern u8   D_80060A5C[];
extern u8   D_80060A64[];
extern char D_80013BB4[];
extern u8   D_800733F0[2][0x6C];
extern u8   D_800734C8[2][0xB0];
extern u8   D_80073628[2][0x24];
extern u8   D_80073670[2][0xE4];
extern u8   D_80073838[2][0xA4];
extern u8   D_80073980[0x208];
/// Stores the result of rand() after each dispatcher tick.
extern s32 D_80073C08;

#endif // MC_H
