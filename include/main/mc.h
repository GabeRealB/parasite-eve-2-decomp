#ifndef MC_H
#define MC_H

#include "common.h"

#include "main/session.h"
#include "main/task.h"

struct _UiObject;

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

/// One row of an item table: the item it holds, the attachment slot that item
/// occupies and its stack count. The tables a `McItemScan` chooses between
/// (`Mc_SaveData.itemRows`, `Gp_ItemTable1`, `Gp_ItemTable2`) are arrays of
/// these rows. A non-zero `attachSlot` marks the row as in use: 1..n is the
/// slot the item occupies in the equipped weapon's or armour's attachment list,
/// and -1 marks the row whose item is the equipped armour itself.
typedef struct {
    u8  itemId;     // Item id; 0 marks the row free
    s8  attachSlot; // Attachment slot the item occupies (-1 = the equipped armour itself)
    u16 qty;        // Stack count
} McItemRec;
STATIC_ASSERT_SIZEOF(McItemRec, 0x4);

/// One entry of `Mc_SaveData`'s per-weapon item table, reached through
/// `Gp_GetItemSlot` and indexed by weapon item id (0x80–0x9F): the ammunition
/// the weapon is loaded with, and the attachment fitted to it.
typedef struct {
    u8  ammoId;    // Ammunition item id (0 = none loaded)
    u8  ammoQty;   // How many of ammoId the weapon holds
    u8  attachId;  // Attachment item id (0xFF = the weapon takes none, 0 = slot empty)
    u8  attachQty; // How many of attachId the weapon holds
    s32 field_4;   // Role unproven: cleared wherever a slot is reset, and no body reads it
} McItemSlot;
STATIC_ASSERT_SIZEOF(McItemSlot, 0x8);

/// Window on the rows of an item table an inventory operation works on: which
/// table, the first row and how many rows. The tables it selects between hold
/// `McItemRec` rows, and each place the game stores items keeps its own window.
/// `Mc_SaveData.carriedItems` holds the window on the items the player carries, so
/// the menus act on that window instead of on a whole table; a scan can also be
/// built locally to search a wider run of rows.
typedef struct {
    u8 firstRow; // First row of the window
    u8 rowCount; // Number of rows the window covers
    u8 table;    // Table the window lies in (0 `Mc_SaveData.itemRows`, 1 `Gp_ItemTable1`, 2 `Gp_ItemTable2`)
    u8 field_3;  // Nothing reads or writes it; role unproven
} McItemScan;
STATIC_ASSERT_SIZEOF(McItemScan, 0x4);

/// One saved pose of a placed enemy: the position and rotation it had when it
/// was taken out of the world, and the state it is to be resumed in.
/// `Mc_SaveData.enemyPoses` holds a fixed run of these, filed by
/// `Gp_SaveEnemyPose` and read back by `Gp_SpawnArea`, so an area the player
/// returns to puts the enemy back where they left it - and an enemy the table
/// holds no record for is not put back at all.
///
/// A record is keyed by `placeKey`, the same key the enemy itself carries, and
/// `spawnState` doubles as the slot's occupancy marker, 0 meaning free. The
/// rotation shares the record with the position, so each angle is kept as the
/// high byte of the angle it was taken from, and shifted back up on restore.
typedef struct {
    u8  pitch;      // Rotation about X, kept as the angle's high byte
    u8  yaw;        // Rotation about Y, likewise
    u8  roll;       // Rotation about Z, likewise
    s8  spawnState; // State the enemy is resumed in; 0 marks the slot free
    s16 x;          // X of the enemy's coordinate, narrowed to 16 bits
    s16 y;          // Y of the enemy's coordinate, likewise
    s16 z;          // Z of the enemy's coordinate, likewise
    u16 placeKey;   // Placement the enemy was spawned from, as `GpEnemy.placeKey`
} McPosRec;
STATIC_ASSERT_SIZEOF(McPosRec, 0xC);

/// The save data: everything a memory card save holds, resident in main's BSS
/// and copied out as one image.
///
/// It carries the run's progress - where the player was, the time played, the
/// player's statistics and the whole inventory with its per-weapon equipment -
/// and the record of what the game has already done: which stages and areas
/// were visited, which placed enemies were taken out of the world and where
/// they were left, which items have been seen, and what the shops still hold in
/// stock. The header block and the data block each carry a checksum pair.
typedef struct {
    byte       unknown_0[0x4];
    GameLoc    at4;               // Place the save was made at, as `GameSession.at4`
    u16        playTime;          // Played time in minutes (capped at 0xEA5F)
    s8         clearCount;        // Times the game has been completed (0 never, capped at 99)
    s8         gameMode;          // Mode the run is played in (0..2); the stat, cost and item-grant tables have one variant per mode
    u8         visitFlags;        // One bit per stage marking it as visited; bit 0 marks the new-game setup as done
    s8         saveNumber;        // Number of this save among those made at the same save point (1..99)
    u8         savePoint;         // Save point the save was made at, indexing the tables of place names the slot and the save header print (1..16)
    s8         companionType;     // Companion the save carries (0 none, otherwise 1..3 index `Gp_AllyIdBase`)
    s32        playerExp;         // Player experience as of the save, as `Player_Status.exp`
    s32        playerBp;          // Player BP as of the save, as `Player_Status.bp`
    u16        hdrChecksum;       // Sum over the header's 0x38 bytes, which start at `at4`
    u16        hdrChecksumInv;    // Complement of `hdrChecksum`, written with it
    byte       unknown_20[0x1];
    s8         vibration;         // Vibration setting (0 on, 1 off)
    s8         characterId;       // Character the save plays as (1-based); the weapon, animation and placement tables are indexed by it
    s8         demoScene;         // Attract demo being played back (0 during normal play)
    byte       unknown_24[0x1];
    u8         moveMode;          // Default movement (0 walk, 1 run)
    u8         hpBonus;           // Addend to the player's maximum HP
    u8         mpBonus;           // Addend to the player's maximum MP
    McPosRec   enemyPoses[0x20];  // The placed enemies taken out of the world, filed by their placement key
    s8         buttonLayout;      // Button layout the pad is remapped through (0..2)
    s8         soundMode;         // Sound output (0 stereo, 1 mono)
    u8         musicVolume;       // Music volume (0..3, 3 is off)
    s8         cursorMode;        // Cursor behaviour in the menus (0 remembers the row, 1 resets it)
    McItemRec  itemRows[0x100];   // The save's own item table, indexed by row; the rows the player carries are `carriedItems`
    s32        collectedBits[4];  // 128 bits, one per collectible the player has picked up
    McItemScan carriedItems;      // Window on the player's rows of `itemRows`
    byte       unknown_5C0[0x2];
    s8         cheatMode;         // While set nothing is spent - no MP for the abilities, no ammunition and no attachments - and the save is left out of the save-slot comparison
    s8         interlace;         // Non-zero runs the display interlaced
    byte       unknown_5C4;
    s8         sceneEvent;        // Scene event a script arms for the music and sound task
    byte       unknown_5C6[0x1];
    s8         companionVariant;  // Which variant of `companionType` is spawned
    McItemSlot weaponItems[0x20]; // Per-weapon equipment, indexed by item id - 0x80: the ammunition loaded and the attachment fitted
    u16        companionHp;       // Companion's current HP
    u16        companionHpMax;    // Companion's maximum HP
    u16        field_6CC;
    u16        field_6CE;
    s32        itemSeenBits[0x60];    // One bit per item id (ids at or above 0x180 count as seen), set once the item was looked at
    u8         attachLevels[0x12];    // Level bought for each Parasite Energy slot (0..3), addressed as page * 3 + column
    s16        attachUseCounts[19];   // Times each Parasite Energy slot was used; the Play Data panel reports the first twelve
    s32        weaponUseCounts[0x20]; // Times each weapon was used, indexed by item id - 0x80
    s8         itemLevelBonus[0x20];  // Addend to the level of items 0x60-0x7F
    byte       unknown_928[0x1];
    s8         field_929;
    s8         replayRank; // Rank the replay earns (0 none, 1, 2)
    u8         saveCount;  // How many times the game has been saved (capped at 99)
    s32        field_92C;
    s32        field_930;
    s32        shopTiers;         // Bitmask of the 13 price tiers the parking-lot shop offers
    u32        shopStock;         // 12 two-bit stock levels for that shop
    u16        dataChecksum;      // Sum over the save's data block
    u16        dataChecksumInv;   // Complement of `dataChecksum`, written with it
    u16        bufferChecksum;    // Sum over the first byte of each memcard buffer slot
    s16        bufferChecksumInv; // Complement of `bufferChecksum`, written with it
} McSaveData;
STATIC_ASSERT_SIZEOF(McSaveData, 0x944);
STATIC_ASSERT(OFFSET_OF(McSaveData, at4) == 4, McSaveData_at4);
STATIC_ASSERT(OFFSET_OF(McSaveData, playTime) == 0xC, McSaveData_playTime);

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

/// Render the selected memory-card slot and its saved statistics.
void func_800330D8(struct _UiObject* obj, s32 work, s32 slot, s32 x, s32 y);
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
extern char Mc_StrMemoryCard[];
/// "*" wildcard passed to MemCardGetDirentry by Mc_StateScanDirFlags.
extern char D_80013A5C[];
/// Jump table of 44 McStateFunc handlers used by Mc_DispatchStateTable.
extern McStateFuncTable44 Mc_PromptStates;
/// Jump table of 26 McStateFunc handlers used by Mc_DispatchStateTable26.
extern McStateFuncTable26 Mc_FileSelectStates;
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

// Save-slot detail labels and indexed descriptions.
extern s8  D_80013998[];
extern s8  D_800139A0[];
extern s8  D_800139A4[];
extern u8* D_800675F0[];
extern u16 Mc_SaveHeaderBody[];
extern u8  D_800139A8[];
extern u8  D_80013B6C[];
extern u8  D_80013B74[];
extern u8  D_80013BA4[];
extern u8  D_80013BA8[];
extern u8  D_80013BAC[];
extern u8  D_80013BB0[];
extern u8  D_80060A48[];
extern u8  D_80060CCC[];
extern u8* D_800611B8[];
extern u8* D_80067418[];

#endif // MC_H
