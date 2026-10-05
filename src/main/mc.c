#include "mc.h"

#include <psyq/sys/types.h>
#include <psyq/sys/file.h>
#include <psyq/kernel.h>
#include <psyq/libmcrd.h>
#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/strings.h>

#include "common.h"

#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag_types.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "ui.h"
#include "main/ui_types.h"
#include "wipsys.h"
#include "main/wipsys_types.h"

/// Serialized preview span, checksum width and place-label values.
enum {
    MEMORY_CARD_SAVE_PREVIEW_FILE_OFFSET   = 0x200,
    MEMORY_CARD_SAVE_HEADER_CHECKSUM_BYTES = 0x38,
    MEMORY_CARD_CHECKSUM_MASK              = 0xFFFF,
    MEMORY_CARD_SAVE_POINT_COUNT           = 16,
    MEMORY_CARD_SAVE_POINT_OPENING         = 15,
};

/// Byte used to distinguish uninitialized backup records from cleared live records.
enum { MEMORY_CARD_BACKUP_FILL_BYTE = 0xFF };

/// Shift from 128-byte card-sector positions to SDK byte offsets.
enum { MEMORY_CARD_SECTOR_BYTE_SHIFT = 7 };

/// Destination states in the save dialog's `Mc_PromptStates` table.
enum {
    MEMORY_CARD_SAVE_STATE_ACCEPT_CARD       = 2,
    MEMORY_CARD_SAVE_STATE_CONFIRM_CREATE    = 7,
    MEMORY_CARD_SAVE_STATE_CONFIRM_SAVE      = 0xE,
    MEMORY_CARD_SAVE_STATE_PREPARE_SECTION   = 0xF,
    MEMORY_CARD_SAVE_STATE_NO_CARD           = 0x14,
    MEMORY_CARD_SAVE_STATE_ACCESS_FAILED     = 0x18,
    MEMORY_CARD_SAVE_STATE_CONFIRM_OVERWRITE = 0x1A,
    MEMORY_CARD_SAVE_STATE_WRITE_FAILED      = 0x2A,
};

/// Destination states in the load dialog's `Mc_FileSelectStates` table.
enum {
    MEMORY_CARD_LOAD_STATE_FAILED          = 6,
    MEMORY_CARD_LOAD_STATE_NO_DATA         = 0xB,
    MEMORY_CARD_LOAD_STATE_PREPARE_SECTION = 0xE,
    MEMORY_CARD_LOAD_STATE_OPEN_PREVIEW    = 0x14,
};

/// The two text lines of one memory-card prompt, drawn one above the other.
///
/// `Mc_PromptTable` has one row per `promptId`. The second line continues the
/// first or adds another sentence; an empty string leaves that line blank.
typedef struct {
    u8* upperLine; // Drawn above the panel content origin.
    u8* lowerLine; // Drawn below `upperLine`.
} McPromptPair;
STATIC_ASSERT_SIZEOF(McPromptPair, 0x8);

/// Generic view of a checksummed save record: a four-byte checksum header
/// followed by the payload it covers.
///
/// Every record the memory-card slots 1..8 save opens with this header (the
/// save image, the player status and the game-flag banks); the record's own
/// type spells the same two fields. The checksum is the low 16 bits of the sum
/// of the payload bytes read as `s8`. Saving stores both halves; verification
/// compares only `checksum`.
typedef struct {
    u16 checksum;           // Sum of the payload bytes as signed bytes, modulo 65536
    u16 checksumComplement; // Ones' complement of `checksum`
    u8  payload[0];         // Record body; its length is the slot's `bytesPerCopy` minus this header
} _McChecksumBlock;
STATIC_ASSERT_SIZEOF(_McChecksumBlock, 4);

/// One section of the save file: a resident record that saving writes to the
/// card twice, back to back, and that loading reads back the same way.
///
/// The sections are stored in table order, each starting on a 128-byte card
/// sector. Section 0 is the card file header (title, CLUT and icon frames),
/// whose 0x200 bytes the walk treats as two 0x100-byte halves; it carries no
/// checksum. Sections 1..8 are the save image, the player status and the
/// game-flag banks: each opens with a `_McChecksumBlock` header, and its
/// resident storage holds the live copy followed by the backup copy.
typedef struct {
    void* buffer;       // Resident storage: the live copy followed by the backup, `bytesPerCopy` bytes each
    s32   bytesPerCopy; // Length of one copy in bytes
    s32   cardSectors;  // 128-byte card sectors the section occupies: both copies, rounded up
} _McSaveSection;
STATIC_ASSERT_SIZEOF(_McSaveSection, 0xC);

/// Handler for one state of a memory-card dialog machine.
///
/// Each run of a dialog task calls the handler its `Task.state` indexes, with
/// that task and the work area the dialogs share. A handler moves the machine
/// on by storing the next state in `task->state`; one that leaves it alone is
/// called again on the next run.
typedef void (*_McStateFunc)(Task* task, McWork* work);

/// Handlers of the save dialog, indexed by `Task.state`.
///
/// The dialog calls the handler for its current state with the task and the
/// shared memory-card work area. A handler advances the dialog by storing the
/// next state. Dispatch indexes the table only when the state is non-negative.
/// The one negative value stored is -1, written when a closing prompt finishes
/// its dismiss count; dispatch does not call a handler for it. The table has
/// no terminator and dispatch does not reject an index past the last handler.
typedef struct {
    _McStateFunc funcs[44]; // One handler per state, in `Task.state` order
} _McSaveStateTable;
STATIC_ASSERT_SIZEOF(_McSaveStateTable, 0xB0);

/// Handlers of the file-select dialog, indexed by `Task.state`.
///
/// The dialog calls the handler for its current state with the task and the
/// shared memory-card work area. A handler advances the dialog by storing the
/// next state. Dispatch requires an index in 0..25. The table has no
/// terminator and the caller does not range-check the index.
typedef struct {
    _McStateFunc funcs[26]; // One handler per state, in `Task.state` order
} _McFileSelectStateTable;
STATIC_ASSERT_SIZEOF(_McFileSelectStateTable, 0x68);

/* Define BSS before API headers to preserve first-declaration order. */
/// Work area of the memory-card dialogs, passed to every state handler.
static McWork Mc_MenuWork;

McSaveData gMcSaveData[MEMORY_CARD_SAVE_COUNT];

GameFlagAcropolisBank GameFlag_AcropolisBanks[2];

GameFlagDryfieldBank GameFlag_DryfieldBanks[2];

GameFlagDryfieldNightBank GameFlag_DryfieldFullBanks[2];

GameFlagMineShelterBank GameFlag_ShelterBanks[2];

GameFlagNeoArkBank GameFlag_NeoArkBanks[2];

GameFlagNibbleBank gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_COUNT];

/// Unreferenced.
static u8 D_80073B80[8];

PlayerStatus gPlayerStatus;

/// Last `rand()` result drawn by `Mc_DispatchStateTable`; nothing reads it.
static s32 Mc_LastRandomValue;

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/wipsys.h"

/// Number of 128-byte card sectors holding the complete game-flag bank pair.
enum { GAME_FLAG_NIBBLE_BANK_CARD_SECTORS = 4 };

/// 128-byte card sectors holding the card file header.
enum { MEMORY_CARD_FILE_HEADER_CARD_SECTORS = 4 };

/// 128-byte card sectors holding both player-status save record copies.
enum { PLAYER_STATUS_CARD_SECTORS = 1 };

/// 128-byte card sectors holding both `GameFlagAcropolisBank` copies.
enum { GAME_FLAG_ACROPOLIS_BANK_CARD_SECTORS = 2 };

/// 128-byte card sectors holding both `GameFlagDryfieldBank` copies.
enum { GAME_FLAG_DRYFIELD_BANK_CARD_SECTORS = 3 };

/// 128-byte card sectors holding both `GameFlagDryfieldNightBank` copies.
enum { GAME_FLAG_DRYFIELD_NIGHT_BANK_CARD_SECTORS = 1 };

/// 128-byte card sectors holding both `GameFlagMineShelterBank` copies.
enum { GAME_FLAG_MINE_SHELTER_BANK_CARD_SECTORS = 4 };

/// 128-byte card sectors holding both `GameFlagNeoArkBank` copies.
enum { GAME_FLAG_NEO_ARK_BANK_CARD_SECTORS = 3 };

extern _McSaveSection Mc_BufferSlots[9];

static const char Mc_StrMemoryCard[];

static u8 McText_NewBlock[];

static u8 McText_Empty[];

static u8 McText_AccessFailed[];

static u8 McText_CheckingCard[];

static u8 McText_CardChanged[];

static u8 McText_InsertCard[];

static u8 McText_NoCard[];

static u8 McText_Saving[];

static u8 McText_Loading[];

static u8 McText_Formatting[];

static u8 McText_SavePrompt[];

static u8 McText_LoadPrompt[];

static u8 McText_FormatPrompt[];

static u8 McText_NoGameData[];

static u8 McText_CreatePrompt[];

static u8 McText_CreatingData[];

static u8 McText_OverwritePrompt[];

static u8 McText_PleaseTryAgain[];

static u8 McText_CardFull[];

static u8 McText_DoNotRemoveCard[];

static u8 McText_InsertAnotherCard[];

static u8 McText_SaveOptional[];

static u8 McText_SelectData[];

static u8 McText_SaveFailed[];

static u8 McText_LoadFailed[];

static u8 McText_FormatFailed[];

static u8 McText_CardNotFormatted[];

static u8 McText_SaveCorrupted[];

static u8 McText_CannotLoadData[];

static u8 McText_LoadAbortedCorrupted[];

/// Unreferenced.
static u8 McText_SlotCorrupted[];

static McPromptPair Mc_PromptTable[];

static u8 Mc_SaveFilePattern[];

static u8 Mc_FileName[0x18];

static u8 Mc_FileNameBuf[0x18];

static u8 Mc_FileNameAlphabet[64];

static u16 Mc_GlyphsUpper[];

static u16 Mc_GlyphsLower[];

static u16 Mc_GlyphsSymbol[];

static u8 Mc_DefaultChecksumSrc[];

static UiListRowCallback Mc_SaveSlotCallbacks[];

static UiListRowCallback Mc_LoadSlotCallbacks[];

static u8* Mc_ModeLabels[];

static UiObjectDesc Mc_SaveListDesc[];

static UiObjectDesc Mc_LoadListDescriptors[];

static const char McText_CloseParen[];

static const _McSaveStateTable Mc_PromptStates;

/// Jump table of 26 _McStateFunc handlers used by Mc_DispatchStateTable26.
static const _McFileSelectStateTable Mc_FileSelectStates;

static void Mc_BuildFileName(u8* arg0, s32 arg1);

static void _mcSeedNewGameRecords(void);

static inline void _mcWriteBlockChecksum(u8* recordBytes, s32 recordByteCount);

/// Prompt + optional choice dialog (Mc_PromptTable[mode]).
static s32 Mc_PromptDialog(Task* task, s32 arg1, s32 unused3);

static s32 Mc_PromptDialogChoice(Task* task, s32 arg1, s32 unused3);

static s32 Mc_PromptDialogSpawn(Task* task, s32 arg1, s32 unused3);

static s32 Mc_PromptDialogFile(Task* task, s32 arg1, s32 unused3);

static inline u16* Mc_EncodeTitleText(s8* arg0, u16* arg1);

static inline u16* Mc_EncodeTitleLiteral(s8* arg0, u16* arg1);

static inline void _mcWriteSaveHeaderChecksum(void);

static inline u16* _mcAppendEncodedTitleLabel(const u8* encodedLabel, u16* titleCursor);

static inline void _mcWriteCardHeaderChecksum(void);

/// Chooses the save number and builds the card title and its checksums.
static void Mc_BuildSaveTitle(McWork* work);

static void Mc_StateScanDirFlags(Task* task, McWork* work);

static void Mc_StateListDirectory(Task* task, McWork* work);

static inline void _mcDrawPrompt(Task* task, s32 promptId);

static inline void _mcCloseChildUi(Task* task, s32 parentInputControl);

/// Inline form of Mc_CopyFileName: 0 saves Mc_FileName to Mc_FileNameBuf, otherwise restores it.
static inline void _mcCopyFileName(s32 arg0);

static void Mc_StateFileSelect(Task* task, McWork* work);

static inline s32 _mcGetSectionWriteMask(void);

static void Mc_StateCompareBuffers(Task* task, McWork* work);

static void _mcStateOpenSaveFileForWrite(Task* task, McWork* work);

static void Mc_StateCreateFile(Task* task, McWork* work);

static void Mc_StatePadFileName(Task* task, McWork* work);

static void Mc_StateNameEntry(Task* task, McWork* work);

static inline void _mcBackupSaveSections(void);

static inline void _mcWriteSectionChecksumSummary(void);

static void Mc_StateBackupBuffers(Task* task, McWork* work);

static void _mcStateFinishSectionWrite(Task* task, McWork* work);

static void Mc_StateFormat(Task* task, McWork* work);

static void Mc_StateSyncFileSelect(Task* task, McWork* work);

static void Mc_StateBlankFileName(Task* task, McWork* work);

static void _mcStateOpenSaveFileForRead(Task* task, McWork* work);

static inline s32 _mcVerifySaveSectionChecksums(void);

static inline void _mcWriteSaveSectionChecksums(void);

static inline s32 _mcVerifySectionChecksumSummary(void);

static void Mc_StateVerifyFinish(Task* task, McWork* work);

static inline void _mcWriteReadCardHeaderChecksum(McWork* work);

static void _mcStateFinishSectionRead(Task* task, McWork* work);

static inline s32 _mcVerifySavePreviewChecksum(const McSavePreview* preview);

static void Mc_StateSaveSlotUi(UiList* list, UiObject* object);

static u16* Mc_EncodeAsciiGlyphs(s8* arg0, u16* arg1);

static void Mc_InitFileName(void);

static void Mc_CopyFileName(s32 arg0);

static void Mc_WriteSaveHdrChecksum(void);

static s32 _mcVerifySaveHeaderChecksum(const McSaveData* save);

/// Out-of-line form of `_mcWriteBlockChecksum`. Nothing calls it.
static void Mc_WriteBlockChecksum(u8* data, s32 size);

static void _mcResetStageFlagRecords(void);

/// Whether a buffer's header holds the sum of its payload, as
/// `Mc_WriteBlockChecksum` stores it. Only the sum is compared, not its
/// complement. Nothing calls it.
static s32 Mc_VerifyBlockChecksum(u8* data, s32 size);

/// Unused memory-card entry point; retained for the original image layout.
static void Mc_UnusedStub(void);

static s32 Mc_CompareBufferHalves(void);

static void Mc_WriteSlotChecksums(void);

static void Mc_WriteFirstByteChecksum(void);

static s32 Mc_VerifyFirstByteChecksum(void);

static s32 Mc_VerifySlotChecksums(void);

static void Mc_DuplicateBuffers(void);

static void Mc_DrawPrompt(Task* task, s32 arg1);

static void Mc_HideChildUi(Task* task);

static void Mc_WriteDataChecksum(s32 arg0, McWork* work);

static s32 Mc_CompareSaveChecksum(McSaveData* save, McWork* work);

static void Mc_ResetWork(Task* task, McWork* work);

static void Mc_WriteSlotChecksumsEx(Task* task, McWork* work);

static void Mc_StateAcceptMode1(Task* task, McWork* work);

static void _mcStatePollCardAdvance(Task* task, McWork* work);

static void Mc_StateDrawPromptAdvance(Task* task, McWork* work);

static void Mc_StatePromptChoiceB(Task* task, McWork* work);

static void Mc_StateDrawPrompt4(Task* task, McWork* work);

static void Mc_StateEnterDialog4(Task* task, McWork* work);

static void _mcStateWriteFileHeader(Task* task, McWork* work);

static void Mc_StatePromptChoiceGeneric(Task* task, McWork* work);

static void _mcStateWriteSection(Task* task, McWork* work);

static void Mc_StateClosePrompt(Task* task, McWork* work);

static void Mc_KillIfCountdown(Task* task, McWork* unused2);

static void Mc_StateSyncPromptFile3(Task* task, McWork* work);

static void Mc_StatePromptChoice9(Task* task, McWork* work);

static void Mc_StateColdBoot(Task* task, McWork* work);

static void Mc_StateSyncPrompt13(Task* task, McWork* work);

static void Mc_StateEnterPrompt0(Task* task, McWork* work);

static void Mc_StatePromptCountdown(Task* task, McWork* work);

static void Mc_StateDrawPromptTo1F(Task* task, McWork* work);

static void Mc_StateCountdownPrompt4(Task* task, McWork* work);

static void Mc_StateDrawPrompt1Advance(Task* task, McWork* work);

static void _mcStateOpenSavePreview(Task* task, McWork* work);

static void Mc_StateReadHeader(Task* task, McWork* work);

static void Mc_StateOpenNext(Task* task, McWork* work);

static void _mcStateDelaySaveRetry(Task* task, McWork* work);

static void _mcStateDelaySaveConfirmation(Task* task, McWork* work);

static void Mc_StateUiCountdownF(Task* task, McWork* work);

static void Mc_StateEnterPromptE(Task* task, McWork* work);

static void Mc_StateEnterPromptD(Task* task, McWork* work);

static void Mc_StateInitWorkDefaults(Task* task, McWork* work);

static void Mc_StateSetOpenDefaults(Task* task, McWork* work);

static void Mc_StateCountdownPrompt(Task* task, McWork* work);

static void Mc_StateCloseReturn(Task* task, McWork* work);

static void Mc_StatePromptTimeout(Task* task, McWork* work);

static void Mc_KillIfCountdownAlt(Task* task, McWork* unused);

static void Mc_StateEnterPromptF(Task* task, McWork* work);

static void Mc_StateAccept(Task* task, McWork* work);

static void Mc_StateSyncPrompt3(Task* task, McWork* work);

static void Mc_StateSyncPromptA(Task* task, McWork* work);

static void Mc_StateDrawCurrentPrompt(Task* task, McWork* work);

static void _mcStateReadSection(Task* task, McWork* work);

static void Mc_StateDrawPrompt1(Task* task, McWork* work);

static void Mc_StateGetDirentry(Task* task, McWork* work);

static void Mc_StateOpenDirEntry(Task* task, McWork* work);

static void Mc_StateReadSlot(Task* task, McWork* work);

static void _mcStateAdvanceLoadPreview(Task* task, McWork* work);

static void Mc_StateEnterPrompt17(Task* task, McWork* work);

static const char McText_CloseParen[];

static const char Mc_StrMemoryCard[] = "Memory Card";

static u8 McText_NewBlock[]             = "New Block";
u8        McText_Yes[]                  = "Yes";
u8        McText_No[]                   = "No";
u8        McText_Cancel[]               = "Cancel";
u8        McText_Ok[]                   = "OK";
static u8 McText_Empty[]                = "";
static u8 McText_AccessFailed[]         = "Failed to access MEMORY CARD.";
static u8 McText_CheckingCard[]         = "Checking MEMORY CARD in slot 1.";
static u8 McText_CardChanged[]          = "MEMORY CARD has been changed.";
static u8 McText_InsertCard[]           = "Insert MEMORY CARD in slot 1.";
static u8 McText_NoCard[]               = "No MEMORY CARD inserted.";
static u8 McText_Saving[]               = "Saving...";
static u8 McText_Loading[]              = "Loading...";
static u8 McText_Formatting[]           = "Formatting...";
static u8 McText_SavePrompt[]           = "Save game data?";
static u8 McText_LoadPrompt[]           = "Load game data?";
static u8 McText_FormatPrompt[]         = "Format MEMORY CARD?";
static u8 McText_NoGameData[]           = "No game data available. Insert";
static u8 McText_CreatePrompt[]         = "Create save data?";
static u8 McText_CreatingData[]         = "Creating save data...";
static u8 McText_OverwritePrompt[]      = "Overwrite data?";
static u8 McText_PleaseTryAgain[]       = "Please try again.";
static u8 McText_CardFull[]             = "MEMORY CARD full. Insert";
static u8 McText_DoNotRemoveCard[]      = "Do not remove MEMORY CARD.";
static u8 McText_InsertAnotherCard[]    = "another MEMORY CARD in slot 1.";
static u8 McText_SaveOptional[]         = "Need not to save now.";
static u8 McText_SelectData[]           = "Select data.";
static u8 McText_SaveFailed[]           = "Save Failed!";
static u8 McText_LoadFailed[]           = "Load Failed!";
static u8 McText_FormatFailed[]         = "Format Failed!";
static u8 McText_CardNotFormatted[]     = "MEMORY CARD not formatted.";
static u8 McText_SaveCorrupted[]        = "Save data corrupted.";
static u8 McText_CannotLoadData[]       = "Cannot load data.";
static u8 McText_LoadAbortedCorrupted[] = "Save data corrupted.\nLoad aborted";
/// Unreferenced.
static u8 McText_SlotCorrupted[] = "Save data corrupted.";

static McPromptPair Mc_PromptTable[] = {
    { McText_AccessFailed, McText_PleaseTryAgain },
    { McText_CheckingCard, McText_DoNotRemoveCard },
    { McText_CardChanged, McText_Empty },
    { McText_NoCard, McText_InsertCard },
    { McText_Saving, McText_DoNotRemoveCard },
    { McText_Loading, McText_DoNotRemoveCard },
    { McText_Formatting, McText_DoNotRemoveCard },
    { McText_SavePrompt, McText_Empty },
    { McText_LoadPrompt, McText_Empty },
    { McText_CardNotFormatted, McText_FormatPrompt },
    { McText_NoGameData, McText_InsertAnotherCard },
    { McText_CreatePrompt, McText_Empty },
    { McText_CreatingData, McText_DoNotRemoveCard },
    { McText_FormatFailed, McText_PleaseTryAgain },
    { McText_SaveFailed, McText_PleaseTryAgain },
    { McText_LoadFailed, McText_PleaseTryAgain },
    { McText_AccessFailed, McText_PleaseTryAgain },
    { McText_OverwritePrompt, McText_Empty },
    { McText_PleaseTryAgain, McText_Empty },
    { McText_CardFull, McText_InsertAnotherCard },
    { McText_SaveOptional, McText_Empty },
    { McText_Empty, McText_Empty },
    { McText_SelectData, McText_Empty },
    { McText_SaveCorrupted, McText_CannotLoadData },
};

static u8  Mc_SaveFilePattern[]    = "BASLUS-01042*";
static u8  Mc_FileName[0x18]       = "BASLUS-01042________";
static u8  Mc_FileNameBuf[0x18]    = "BASLUS-01042________";
static u8  Mc_FileNameAlphabet[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789;:";
static u16 Mc_GlyphsUpper[]        = {
    0x6082,
    0x6182,
    0x6282,
    0x6382,
    0x6482,
    0x6582,
    0x6682,
    0x6782,
    0x6882,
    0x6982,
    0x6A82,
    0x6B82,
    0x6C82,
    0x6D82,
    0x6E82,
    0x6F82,
    0x7082,
    0x7182,
    0x7282,
    0x7382,
    0x7482,
    0x7582,
    0x7682,
    0x7782,
    0x7882,
    0x7982,
    0x0000,
    0x0000,
};
static u16 Mc_GlyphsLower[] = {
    0x8182,
    0x8282,
    0x8382,
    0x8482,
    0x8582,
    0x8682,
    0x8782,
    0x8882,
    0x8982,
    0x8A82,
    0x8B82,
    0x8C82,
    0x8D82,
    0x8E82,
    0x8F82,
    0x9082,
    0x9182,
    0x9282,
    0x9382,
    0x9482,
    0x9582,
    0x9682,
    0x9782,
    0x9882,
    0x9982,
    0x9A82,
    0x0000,
    0x0000,
};
static u16 Mc_GlyphsSymbol[] = {
    0x4081,
    0x4981,
    0x6881,
    0x9481,
    0x9081,
    0x9381,
    0x9581,
    0x6681,
    0x6981,
    0x6A81,
    0x9681,
    0x7B81,
    0x4381,
    0x7C81,
    0x4481,
    0x5E81,
    0x4F82,
    0x5082,
    0x5182,
    0x5282,
    0x5382,
    0x5482,
    0x5582,
    0x5682,
    0x5782,
    0x5882,
    0x4681,
    0x4781,
    0x8381,
    0x8181,
    0x8481,
    0x4881,
    0x9781,
    0x0000,
};

static u8 Mc_DefaultChecksumSrc[] = {
#include "assets/mc_save_header.inc"
};

_McSaveSection Mc_BufferSlots[9] = {
    { Mc_DefaultChecksumSrc, sizeof(Mc_DefaultChecksumSrc) / 2, MEMORY_CARD_FILE_HEADER_CARD_SECTORS },
    { &gMcSaveData[MEMORY_CARD_SAVE_LIVE], sizeof(McSaveData), MEMORY_CARD_SAVE_CARD_SECTORS },
    { &gPlayerStatus, PLAYER_STATUS_SAVE_RECORD_BYTES, PLAYER_STATUS_CARD_SECTORS },
    { GameFlag_AcropolisBanks, GAME_FLAG_ACROPOLIS_BANK_BYTES, GAME_FLAG_ACROPOLIS_BANK_CARD_SECTORS },
    { GameFlag_DryfieldBanks, GAME_FLAG_DRYFIELD_BANK_BYTES, GAME_FLAG_DRYFIELD_BANK_CARD_SECTORS },
    { GameFlag_DryfieldFullBanks, GAME_FLAG_DRYFIELD_NIGHT_BANK_BYTES, GAME_FLAG_DRYFIELD_NIGHT_BANK_CARD_SECTORS },
    { GameFlag_ShelterBanks, GAME_FLAG_MINE_SHELTER_BANK_BYTES, GAME_FLAG_MINE_SHELTER_BANK_CARD_SECTORS },
    { GameFlag_NeoArkBanks, GAME_FLAG_NEO_ARK_BANK_BYTES, GAME_FLAG_NEO_ARK_BANK_CARD_SECTORS },
    { &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE], sizeof(gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE]), GAME_FLAG_NIBBLE_BANK_CARD_SECTORS },
};

static UiListRowCallback Mc_SaveSlotCallbacks[] = { Mc_StateSaveSlotUi };
UiList                   Mc_SaveSlotList        = { Mc_SaveSlotCallbacks, 0x0F, 0x0F, 0, 0x2E };
static UiListRowCallback Mc_LoadSlotCallbacks[] = { McMenu_ConfirmWithRender };
UiList                   Mc_LoadSlotList        = { Mc_LoadSlotCallbacks, 0x0F, 0x0F, 0, 0x2E };

static u8* Mc_ModeLabels[] = { (u8*)McText_Replay, (u8*)McText_Bounty, (u8*)McText_Scavenger, (u8*)McText_Nightmare };

UiObjectDesc Mc_TaskDescriptors[] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -25, 0x120, 0x32 }, 0x14, 0, TASK_BODY_NONE, 0xC0, Mc_DispatchStateTable26, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -25, 0x120, 0x32 }, 0x14, 0, TASK_BODY_NONE, 0xC0, Mc_DispatchStateTable, 0 },
};
static UiObjectDesc Mc_SaveListDesc[] = {
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { -136, 10, 0x120, 0x3C }, 0x0C, 0, TASK_BODY_NONE, 0xC0, McMenu_SelectList, 0 },
};
static UiObjectDesc Mc_LoadListDescriptors[] = {
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { -136, 10, 0x120, 0x3C }, 0x0C, 0, TASK_BODY_NONE, 0xC0, McMenu_SelectListAlt, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -60, 30, 0xC8, 0x3C }, 0x1C, 0, TASK_BODY_NONE, 0xC0, McMenu_FileInformation, 0 },
};

static void Mc_BuildFileName(u8* arg0, s32 arg1)
{
    s32 i;

    i = 0;
    do {
        *arg0 = Mc_SaveFilePattern[i];
        i++;
        arg0++;
    } while (i < 0xC);

    *arg0   = Mc_FileNameAlphabet[arg1];
    *++arg0 = Mc_FileNameAlphabet[rand() & 0x3F];
    *++arg0 = Mc_FileNameAlphabet[rand() & 0x3F];
    *++arg0 = Mc_FileNameAlphabet[rand() & 0x3F];
    *++arg0 = Mc_FileNameAlphabet[rand() & 0x3F];
    *++arg0 = Mc_FileNameAlphabet[rand() & 0x3F];
    *++arg0 = Mc_FileNameAlphabet[rand() & 0x3F];
    *++arg0 = Mc_FileNameAlphabet[rand() & 0x3F];
    arg0[1] = 0;
}

/// Zero the five live stage-flag records and fill each adjacent backup with 0xFF.
static inline void _mcResetStageFlagCopies(void)
{
    GameFlagAcropolisBank*     acropolisBanks;
    GameFlagDryfieldBank*      dryfieldBanks;
    GameFlagDryfieldNightBank* dryfieldNightBanks;
    GameFlagMineShelterBank*   mineShelterBanks;
    GameFlagNeoArkBank*        neoArkBanks;

    acropolisBanks = GameFlag_AcropolisBanks;
    memFillBytes(acropolisBanks, 0, sizeof(*acropolisBanks));
    dryfieldBanks = GameFlag_DryfieldBanks;
    memFillBytes(dryfieldBanks, 0, sizeof(*dryfieldBanks));
    dryfieldNightBanks = GameFlag_DryfieldFullBanks;
    memFillBytes(dryfieldNightBanks, 0, sizeof(*dryfieldNightBanks));
    mineShelterBanks = GameFlag_ShelterBanks;
    memFillBytes(mineShelterBanks, 0, sizeof(*mineShelterBanks));
    neoArkBanks = GameFlag_NeoArkBanks;
    memFillBytes(neoArkBanks, 0, sizeof(*neoArkBanks));
    memFillBytes(acropolisBanks + 1, MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*acropolisBanks));
    memFillBytes(dryfieldBanks + 1, MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*dryfieldBanks));
    memFillBytes(dryfieldNightBanks + 1, MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*dryfieldNightBanks));
    memFillBytes(mineShelterBanks + 1, MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*mineShelterBanks));
    memFillBytes(neoArkBanks + 1, MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*neoArkBanks));
}

/// Restore option bytes in the live save and immediately apply the audio defaults.
static inline void _mcRestoreOptionDefaults(void)
{
    enum {
        MEMORY_CARD_OPTION_VIBRATION_ON      = 0,
        MEMORY_CARD_OPTION_BUTTON_LAYOUT_A   = 0,
        MEMORY_CARD_OPTION_MUSIC_FULL_VOLUME = 0,
        MEMORY_CARD_OPTION_CURSOR_MEMORY     = 0,
        MEMORY_CARD_OPTION_MOVEMENT_WALK     = 0,
        MEMORY_CARD_OPTION_SOUND_STEREO      = 0,
    };

    McSaveData* save;

    save                     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    save->state.vibration    = MEMORY_CARD_OPTION_VIBRATION_ON;
    save->state.buttonLayout = MEMORY_CARD_OPTION_BUTTON_LAYOUT_A;
    save->state.musicVolume  = MEMORY_CARD_OPTION_MUSIC_FULL_VOLUME;
    save->state.cursorMode   = MEMORY_CARD_OPTION_CURSOR_MEMORY;
    save->state.soundMode    = MEMORY_CARD_OPTION_SOUND_STEREO;
    save->state.moveMode     = MEMORY_CARD_OPTION_MOVEMENT_WALK;
    sndOutputSetStereo(SOUND_OUTPUT_STEREO);
    midiApplyMusicVolume(MIDI_MUSIC_VOLUME_SAVED);
}

/// Reset player and game-flag record pairs, then seed the opening player state.
///
/// Live records are zeroed and backups filled with 0xFF, without recomputing
/// checksums. The live save must already have been cleared. Sets the opening
/// shooting-gallery location and primary character, then seeds player stats.
/// The character index read after seeding is zero: the seeder leaves it at 1.
static void _mcSeedNewGameRecords(void)
{
    enum {
        MEMORY_CARD_NEW_GAME_VIEW        = 1,
        MEMORY_CARD_NEW_GAME_ROOM        = 1,
        MEMORY_CARD_NEW_GAME_WARP        = 7,
        MEMORY_CARD_NEW_GAME_VARIANT     = 1,
        MEMORY_CARD_NEW_GAME_SCENE_EVENT = 2,
        MEMORY_CARD_NEW_GAME_CHARACTER   = 1,
        MEMORY_CARD_NEW_GAME_WEAPON_M93R = 2,
    };

    McSaveData* openingSave;
    s32         initialStage;
    s32         initialSceneEvent;
    s32         starterWeapon;
    s32         characterIndex;

    memFillBytes(&gPlayerStatus, 0, PLAYER_STATUS_SAVE_RECORD_BYTES);
    memFillBytes(gPlayerStatus.saveBackup, MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(gPlayerStatus.saveBackup));
    memFillBytes(&gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE], 0, sizeof(gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE]));
    memFillBytes(&gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_BACKUP], MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_BACKUP]));

    // Reset every record pair before seeding the opening location and player.
    do {
        _mcResetStageFlagCopies();
        openingSave = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    } while (0);

    initialStage                            = GAME_STAGE_ACROPOLIS;
    openingSave->state.location.loc.area    = GAME_AREA_MIST_SHOOTING_GALLERY;
    initialSceneEvent                       = MEMORY_CARD_NEW_GAME_SCENE_EVENT;
    starterWeapon                           = MEMORY_CARD_NEW_GAME_WEAPON_M93R;
    openingSave->state.location.loc.stage   = initialStage;
    openingSave->state.location.loc.view    = MEMORY_CARD_NEW_GAME_VIEW;
    openingSave->state.location.loc.room    = MEMORY_CARD_NEW_GAME_ROOM;
    openingSave->state.location.loc.warp    = MEMORY_CARD_NEW_GAME_WARP;
    openingSave->state.location.loc.variant = MEMORY_CARD_NEW_GAME_VARIANT;
    openingSave->state.sceneEvent           = initialSceneEvent;
    openingSave->state.characterId          = MEMORY_CARD_NEW_GAME_CHARACTER;
    playerSeedNewGameStatus();
    characterIndex                          = openingSave->state.characterId - 1;
    (&gPlayerStatus)[characterIndex].weapon = starterWeapon;
}

/// Write the signed-byte payload sum and its complement into a save record.
///
/// `recordByteCount` includes the four-byte `_McChecksumBlock` header and must
/// be at least that large. `recordBytes` must point to a writable, halfword-aligned
/// record of that extent. Each addition retains its low 16 bits.
static inline void _mcWriteBlockChecksum(u8* recordBytes, s32 recordByteCount)
{
    _McChecksumBlock* block;
    s16               sum;
    u32               byteIndex;

    block            = (_McChecksumBlock*)recordBytes;
    sum              = 0;
    recordBytes      = block->payload;
    recordByteCount -= sizeof(_McChecksumBlock);
    byteIndex        = 0;
    if (recordByteCount != 0) {
        do {
            byteIndex   += 1;
            sum         += (s8)*recordBytes;
            recordBytes += 1;
        } while (byteIndex < recordByteCount);
    }
    block->checksum           = sum;
    block->checksumComplement = ~sum;
}

void mcResetSaveData(void)
{
    enum { MEMORY_CARD_DEFAULT_SPRITE_VARIANT = 1,
           MEMORY_CARD_BACKUP_ALL_BITS        = -1 };

    _McSaveSection* sections;
    _McSaveSection* section;
    u8*             recordByte;
    u32             bytesPerCopy;
    u32             byteIndex;
    s32             backupFill;

    // Section zero is the title and icon header; only record pairs are reset.
    backupFill = MEMORY_CARD_BACKUP_ALL_BITS;
    sections   = Mc_BufferSlots;
    section    = sections + 1;
    do {
        bytesPerCopy = section->bytesPerCopy;
        recordByte   = section->buffer;
        for (byteIndex = 0; byteIndex < bytesPerCopy; byteIndex++) {
            *recordByte++ = 0;
        }
        for (byteIndex = 0; byteIndex < bytesPerCopy; byteIndex++) {
            *recordByte++ = backupFill;
        }
        _mcWriteBlockChecksum(section->buffer, bytesPerCopy);
        section++;
    } while (section < sections + ARRAY_SIZE(Mc_BufferSlots));

    gDisplayState.spriteVariant = MEMORY_CARD_DEFAULT_SPRITE_VARIANT;
    _mcSeedNewGameRecords();

    _mcRestoreOptionDefaults();
}

/// Prompt + optional choice dialog (Mc_PromptTable[mode]).
static s32 Mc_PromptDialog(Task* task, s32 arg1, s32 unused3)
{
    u32            textColorRgb;
    s32            one;
    UiObject*      obj;
    register Task* childTask asm("a0");
    UiObject*      childObject;
    McPromptPair*  entry;
    McPromptPair*  base;

    obj          = task->spawnArg2.pointer;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);

    childTask = task->firstChild;
    if (childTask == NULL) {
        childObject = Ui_SpawnFromDesc(Mc_PromptDesc, one, one, 2, obj);
        if (childObject != NULL) {
            childObject->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 5) - childObject->panel.bounds.unsignedRect.w;
            childObject->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue + 8;
            obj->resultValue                         = 0;
            obj->panel.control.word                  = USER_INTERFACE_PANEL_INACTIVE;
        }
        return 0;
    }
    // Borrow the child task's separate UI object until closing begins.
    childObject = childTask->spawnArg2.pointer;
    if (childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
        obj->resultValue = childObject->resultValue;
        uiStartTreeClosing(childObject, childObject->owner);
        obj->panel.control.word = one;
    }
    return obj->resultValue;
}

static s32 Mc_PromptDialogChoice(Task* task, s32 arg1, s32 unused3)
{
    u32            textColorRgb;
    s32            one;
    UiObject*      obj;
    register Task* childTask asm("a0");
    UiObject*      childObject;
    McPromptPair*  entry;
    McPromptPair*  base;

    obj          = task->spawnArg2.pointer;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);

    childTask = task->firstChild;
    if (childTask == NULL) {
        childObject = Ui_SpawnFromDesc(Mc_PromptDesc, 0, one, 2, obj);
        if (childObject != NULL) {
            childObject->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 5) - childObject->panel.bounds.unsignedRect.w;
            childObject->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue + 0x10;
            obj->resultValue                         = 0;
            obj->panel.control.word                  = USER_INTERFACE_PANEL_INACTIVE;
        }
        return 0;
    }
    // Borrow the child task's separate UI object until closing begins.
    childObject = childTask->spawnArg2.pointer;
    if (childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
        obj->resultValue = childObject->resultValue;
        uiStartTreeClosing(childObject, childObject->owner);
        obj->panel.control.word = one;
    }
    return obj->resultValue;
}

static s32 Mc_PromptDialogSpawn(Task* task, s32 arg1, s32 unused3)
{
    u32            textColorRgb;
    s32            one;
    UiObject*      obj;
    register Task* childTask asm("a0");
    UiObject*      childObject;
    McPromptPair*  entry;
    McPromptPair*  base;

    obj          = task->spawnArg2.pointer;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);

    childTask = task->firstChild;
    if (childTask == NULL) {
        childObject = Ui_SpawnFromDesc(Mc_PromptDesc, 3, one, 2, obj);
        if (childObject != NULL) {
            childObject->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 5) - childObject->panel.bounds.unsignedRect.w;
            childObject->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue + 0x10;
            obj->resultValue                         = 0;
            obj->panel.control.word                  = USER_INTERFACE_PANEL_INACTIVE;
        }
        return 0;
    }
    // Borrow the child task's separate UI object until closing begins.
    childObject = childTask->spawnArg2.pointer;
    if (childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
        obj->resultValue = childObject->resultValue;
        uiStartTreeClosing(childObject, childObject->owner);
        obj->panel.control.word = one;
    }
    return obj->resultValue;
}

static s32 Mc_PromptDialogFile(Task* task, s32 arg1, s32 unused3)
{
    u32            textColorRgb;
    s32            one;
    UiObject*      obj;
    register Task* childTask asm("a0");
    UiObject*      childObject;
    McPromptPair*  entry;
    McPromptPair*  base;

    obj          = task->spawnArg2.pointer;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);

    childTask = task->firstChild;
    if (childTask == NULL) {
        childObject = Ui_SpawnFromDesc(Mc_PromptDesc, 2, one, 2, obj);
        if (childObject != NULL) {
            childObject->panel.bounds.unsignedRect.h = 0x12;
            childObject->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 5) - childObject->panel.bounds.unsignedRect.w;
            childObject->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue + 8;
            obj->resultValue                         = 0;
            obj->panel.control.word                  = USER_INTERFACE_PANEL_INACTIVE;
        }
        return 0;
    }
    // Borrow the child task's separate UI object until closing begins.
    childObject = childTask->spawnArg2.pointer;
    if (childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
        obj->resultValue = childObject->resultValue;
        uiStartTreeClosing(childObject, childObject->owner);
        obj->panel.control.word = one;
    }
    return obj->resultValue;
}

static inline u16* Mc_EncodeTitleText(s8* arg0, u16* arg1)
{
    s32 ch;
    u16 idx;
    u8  ch_u;

    ch_u = *arg0;
    if (*arg0 != 0) {
        do {
            ch = (s8)ch_u;
            if (ch >= 0x61) {
                idx   = Mc_GlyphsLower[ch - 0x61];
                *arg1 = idx;
            } else if (ch >= 0x41) {
                idx   = Mc_GlyphsUpper[ch - 0x41];
                *arg1 = idx;
            } else if (ch >= 0x20) {
                idx   = Mc_GlyphsSymbol[ch - 0x20];
                *arg1 = idx;
            }
            arg0++;
            ch_u = *arg0;
            arg1++;
        } while (*arg0 != 0);
    }
    *arg1 = 0;
    return arg1;
}

static inline u16* Mc_EncodeTitleLiteral(s8* arg0, u16* arg1)
{
    s32 ch;
    u16 idx;

    if (*arg0 != 0) {
        do {
            ch = *arg0;
            if (ch >= 0x61) {
                idx   = Mc_GlyphsLower[ch - 0x61];
                *arg1 = idx;
            } else if (ch >= 0x41) {
                idx   = Mc_GlyphsUpper[ch - 0x41];
                *arg1 = idx;
            } else if (ch >= 0x20) {
                idx   = Mc_GlyphsSymbol[ch - 0x20];
                *arg1 = idx;
            }
            arg0++;
            arg1++;
        } while (*arg0 != 0);
    }
    *arg1 = 0;
    return arg1;
}

/// Write the live save's preview checksum and complement.
///
/// Covers `MEMORY_CARD_SAVE_HEADER_CHECKSUM_BYTES` signed bytes from `location`,
/// including the checksum pair. Initialize that pair to zero and all ones so
/// its signed-byte contribution stays -2 when the result is stored.
static inline void _mcWriteSaveHeaderChecksum(void)
{
    u16       sum;
    const u8* headerByte;
    s32       checksumByteCount;
    s32       byteIndex;
    s16       signedByte;

    sum                                                               = 0;
    headerByte                                                        = (const u8*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    headerByte                                                       += OFFSET_OF(McSavePreview, location);
    checksumByteCount                                                 = MEMORY_CARD_SAVE_HEADER_CHECKSUM_BYTES;
    byteIndex                                                         = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.headerChecksum           = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.headerChecksumComplement = MEMORY_CARD_CHECKSUM_MASK;
    do {
        byteIndex  += 1;
        signedByte  = (s8)*headerByte;
        sum         = sum + signedByte;
        headerByte += 1;
    } while (byteIndex < checksumByteCount);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.headerChecksum           = sum;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.headerChecksumComplement = MEMORY_CARD_CHECKSUM_MASK - (u32)sum;
    _mcVerifySaveHeaderChecksum(&gMcSaveData[MEMORY_CARD_SAVE_LIVE]);
}

/// Append a zero-terminated Shift-JIS place label to the card-title cursor.
///
/// `encodedLabel` contains complete two-byte glyphs with no embedded zero bytes.
/// `titleCursor` must be halfword-aligned and have room for the label and its
/// byte terminator; source and destination must not overlap. Returns the
/// cursor at that terminator for the next append;
/// the copy preserves encoded bytes without converting them.
static inline u16* _mcAppendEncodedTitleLabel(const u8* encodedLabel, u16* titleCursor)
{
    u8* destinationByte = (u8*)titleCursor;

    while (*encodedLabel != 0) {
        *destinationByte++ = *encodedLabel++;
    }
    *destinationByte = 0;
    return (u16*)destinationByte;
}

/// Store the complete card file header's signed-byte sum in the live save.
///
/// Covers all title, CLUT and icon bytes in `Mc_DefaultChecksumSrc`, retaining
/// the low 16 bits and storing their complement in the saved checksum pair.
static inline void _mcWriteCardHeaderChecksum(void)
{
    u16       sum;
    s32       headerByteCount;
    const u8* headerByte;
    u16*      checksum;
    s32       byteIndex;

    sum                                                                      = 0;
    headerByteCount                                                          = sizeof(Mc_DefaultChecksumSrc);
    headerByte                                                               = Mc_DefaultChecksumSrc;
    checksum                                                                 = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.titleChecksum;
    byteIndex                                                                = 0;
    *checksum                                                                = sum;
    PARENT_OF(checksum, McSaveState, titleChecksum)->titleChecksumComplement = MEMORY_CARD_CHECKSUM_MASK - (u32)sum;
    do {
        byteIndex  += 1;
        sum        += (s8)*headerByte;
        headerByte += 1;
    } while (byteIndex < headerByteCount);
    *checksum                                                                = sum;
    PARENT_OF(checksum, McSaveState, titleChecksum)->titleChecksumComplement = MEMORY_CARD_CHECKSUM_MASK - (u32)sum;
}

static void Mc_BuildSaveTitle(McWork* work)
{
    u8             buffer[0x20];
    u16*           title;
    s32            number;
    s32            i;
    s32            candidate;
    s32            available;
    McSavePreview* slot;

    title  = (u16*)(Mc_DefaultChecksumSrc + 4);
    number = 1;
    if (work->entryCount > 0) {
        for (i = 0; i < work->entryCount; i++) {
            slot = &work->previews[i];
            if ((s8)slot->savePoint == (s8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint) {
                if (slot->saveNumber >= number) {
                    number = slot->saveNumber + 1;
                }
            }
        }
        if (number >= 100) {
            for (candidate = 1; candidate < 100; candidate++) {
                available = 1;
                for (i = 0; i < work->entryCount; i++) {
                    slot = &work->previews[i];
                    if ((s8)slot->savePoint == (s8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint && slot->saveNumber == candidate) {
                        available = 0;
                        break;
                    }
                }
                if (available == 1) {
                    number = candidate;
                    break;
                }
            }
        }
    }
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveNumber = number;
    title                                               = Mc_EncodeTitleLiteral("PE2 ", title);
    title                                               = Mc_EncodeTitleText((s8*)textFormatPlayTime(buffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime), title);
    title                                               = Mc_EncodeTitleLiteral(" ", title);
    Mc_DefaultChecksumSrc[0x43]                         = 0;
    Mc_DefaultChecksumSrc[0x42]                         = 0;
    title                                               = _mcAppendEncodedTitleLabel(Mc_LocationTitleLabels[(s8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint], title);
    title                                               = Mc_EncodeTitleLiteral("(", title);
    title                                               = Mc_EncodeTitleText((s8*)textItoaSigned(buffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveNumber), title);
    title                                               = Mc_EncodeTitleLiteral((s8*)McText_CloseParen, title);
    *title                                              = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount == 0xFF) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount = 0;
    } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount < 99) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount++;
    }
    _mcWriteSaveHeaderChecksum();
    _mcWriteCardHeaderChecksum();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksum           = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksumComplement = 0xFFFF;
}

static const char McText_CloseParen[] = ")";

static const _McSaveStateTable Mc_PromptStates = { {
    Mc_ResetWork,
    Mc_WriteSlotChecksumsEx,
    Mc_StateAcceptMode1,
    _mcStatePollCardAdvance,
    Mc_StateCompareBuffers,
    Mc_StateDrawPromptAdvance,
    _mcStateOpenSaveFileForWrite,
    Mc_StatePromptChoiceB,
    Mc_StateDrawPrompt4,
    Mc_StateCreateFile,
    Mc_StateEnterDialog4,
    _mcStateWriteFileHeader,
    _mcStatePollCardAdvance,
    Mc_StatePadFileName,
    Mc_StatePromptChoiceGeneric,
    Mc_StateBackupBuffers,
    _mcStateWriteSection,
    _mcStatePollCardAdvance,
    _mcStateFinishSectionWrite,
    Mc_StateClosePrompt,
    Mc_StateSyncPromptFile3,
    Mc_StatePromptChoice9,
    Mc_StateColdBoot,
    Mc_StateFormat,
    Mc_StateEnterPrompt0,
    Mc_StateSyncPrompt13,
    Mc_StateNameEntry,
    Mc_StatePromptCountdown,
    Mc_StateDrawPromptTo1F,
    Mc_StateCountdownPrompt4,
    Mc_KillIfCountdown,
    Mc_StateDrawPrompt1Advance,
    Mc_StateScanDirFlags,
    Mc_StateListDirectory,
    _mcStateOpenSavePreview,
    Mc_StateReadHeader,
    _mcStatePollCardAdvance,
    Mc_StateOpenNext,
    Mc_StateFileSelect,
    _mcStateDelaySaveRetry,
    Mc_StateUiCountdownF,
    _mcStateDelaySaveConfirmation,
    Mc_StateEnterPromptE,
    Mc_StateEnterPromptD,
} };

static void Mc_StateScanDirFlags(Task* task, McWork* work)
{
    u32       textColorRgb;
    UiObject* obj;
    s32       i;
    s32       j;
    s32       size;
    s32       blocks;
    s32       head;
    s32       idx;

    work->cardTimer -= 1;
    if (work->cardTimer == 0) {
        work->entryCount = 0;
        // Mark the fifteen usable blocks free, then claim each file's blocks.
        for (i = 0; i < MEMORY_CARD_BLOCK_COUNT; i++) {
            work->blockOwners[i] = MEMORY_CARD_BLOCK_FREE;
        }
        MemCardGetDirentry(
            work->channel, "*", work->directory, &work->entryCount, 0,
            MEMORY_CARD_DIRECTORY_CAPACITY);

        work->foreignBlockCount = 0;
        if (work->entryCount != 0) {
            for (i = 0; i < work->entryCount; i++) {
                size   = work->directory[i].size;
                head   = work->directory[i].head;
                blocks = size / MEMORY_CARD_BLOCK_BYTES + ((size % MEMORY_CARD_BLOCK_BYTES) != 0);
                // `head` counts 128-byte sectors. Block 0 is the card directory.
                head /= MEMORY_CARD_SECTORS_PER_BLOCK;
                head -= 1;
                for (j = 0; j < blocks; j++) {
                    work->blockOwners[head + j] = MEMORY_CARD_BLOCK_FOREIGN;
                }
                work->foreignBlockCount += blocks;
            }
        }
        task->state += 1;
    }

    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, Mc_PromptTable[idx].upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, Mc_PromptTable[idx].lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

static void Mc_StateListDirectory(Task* task, McWork* work)
{
    u32           textColorRgb;
    s32           one;
    s32           var_s0;
    s32           temp_v0;
    s32           temp_v0_2;
    s32           var_v0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    work->entryCount = 0;
    MemCardGetDirentry(
        work->channel, (char*)Mc_SaveFilePattern, work->directory, &work->entryCount, 0,
        MEMORY_CARD_DIRECTORY_CAPACITY);
    // This game's files count as one block each, so what remains belongs to other products.
    temp_v0                 = work->foreignBlockCount - work->entryCount;
    work->foreignBlockCount = temp_v0;
    if (temp_v0 == MEMORY_CARD_BLOCK_COUNT) {
        var_v0 = 0x19;
    } else {
        if (work->slotWriteMask == MEMORY_CARD_SLOT_WRITE_ALL) {
            work->selectedSlot = 0;
        } else {
            temp_v0_2          = work->entryCount;
            work->selectedSlot = 0;
            if (temp_v0_2 != 0) {
                var_s0 = 0;
                if (temp_v0_2 > 0) {
                    do {
                        if (strncmp(work->directory[var_s0].name, (char*)Mc_FileName, 0x14) == 0) {
                            work->selectedSlot = var_s0;
                            break;
                        }
                        temp_v0_2 = work->entryCount;
                        var_s0   += 1;
                    } while (var_s0 < temp_v0_2);
                }
            }
        }
        work->currentSlot = 0;
        if (work->entryCount > 0) {
            var_v0 = task->state + 1;
        } else {
            var_v0 = 0x26;
        }
    }
    task->state = var_v0;

    /* Map each file's first block back to its directory entry. */
    if (work->entryCount > 0) {
        s32 i;

        for (i = 0; i < work->entryCount; i++) {
            s32 head                = work->directory[i].head;
            head                   /= MEMORY_CARD_SECTORS_PER_BLOCK;
            head                   -= 1;
            work->blockOwners[head] = i;
        }
    }

    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
}

/// Draw one memory-card prompt on the dialog task's panel and clear its UI result.
///
/// `task->spawnArg2.pointer` must borrow a live `UiObject`; `promptId` must index
/// `Mc_PromptTable`. Draws the title and both prompt lines in the normal text
/// color, with outlines and left alignment.
static inline void _mcDrawPrompt(Task* task, s32 promptId)
{
    u32           textColorRgb;
    UiObject*     panelObject;
    McPromptPair* prompt;
    McPromptPair* prompts;

    panelObject         = task->spawnArg2.pointer;
    textColorRgb        = uiGetTextColor(panelObject, USER_INTERFACE_TEXT_COLOR_NORMAL);
    panelObject->result = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(panelObject)->panel, Mc_StrMemoryCard);
    prompts = Mc_PromptTable;
    prompt  = &prompts[promptId];
    textDrawUiLine(panelObject, panelObject->panel.contentLeft.signedValue + 2, -2, prompt->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(panelObject, panelObject->panel.contentLeft.signedValue + 2, 0xF, prompt->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

/// Start closing the first child panel and set the parent panel's input control.
///
/// Does nothing without a child task. Both tasks must borrow live `UiObject`s
/// through `spawnArg2.pointer`. The child becomes inactive before its UI tree
/// starts closing; `parentInputControl` is written to the parent's control word.
static inline void _mcCloseChildUi(Task* task, s32 parentInputControl)
{
    Task*     child;
    UiObject* childPanel;
    UiObject* parentPanel;

    child = task->firstChild;
    if (child != NULL) {
        childPanel                     = child->spawnArg2.pointer;
        parentPanel                    = task->spawnArg2.pointer;
        childPanel->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        uiStartTreeClosing(childPanel, childPanel->owner);
        parentPanel->panel.control.word = parentInputControl;
    }
}

/// Inline form of Mc_CopyFileName: 0 saves Mc_FileName to Mc_FileNameBuf, otherwise restores it.
static inline void _mcCopyFileName(s32 arg0)
{
    u8* src;
    u8* dst;
    s32 i;

    if (arg0 == 0) {
        src = Mc_FileName;
        dst = Mc_FileNameBuf;
    } else {
        src = Mc_FileNameBuf;
        dst = Mc_FileName;
    }

    for (i = 0; i < 0x15; i++) {
        *dst++ = *src++;
    }
}

static void Mc_StateFileSelect(Task* task, McWork* work)
{
    UiObject* obj;
    UiObject* childObj;
    Task*     child;
    s32       syncResult;
    s32       i;

    obj            = task->spawnArg2.pointer;
    work->promptId = MEMORY_CARD_PROMPT_SELECT;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_SELECT);

    child = task->firstChild;
    if (child == NULL) {
        if (Ui_SpawnFromDesc(Mc_LoadListDescriptors, work, 1, 2, obj) != 0) {
            Mc_LoadSlotList.itemCount = work->entryCount;
            if (work->entryCount < MEMORY_CARD_BLOCK_COUNT - work->foreignBlockCount) {
                Mc_LoadSlotList.itemCount++;
            }
            Mc_LoadSlotList.selectedItemIndex = work->selectedSlot;
            obj->resultValue                  = 0;
            obj->panel.control.word           = USER_INTERFACE_PANEL_INACTIVE;
        }
    } else {
        childObj = child->spawnArg2.pointer;
        if (childObj->result == USER_INTERFACE_RESULT_CONFIRM) {
            obj->resultValue             = childObj->resultValue;
            childObj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            uiStartTreeClosing(childObj, childObj->owner);
            obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            if (obj->resultValue >= 0) {
                if (obj->resultValue < work->entryCount) {
                    u8* src;
                    u8* name;
                    s32 matchCount;

                    src        = (u8*)work->directory[obj->resultValue].name;
                    name       = Mc_FileName;
                    matchCount = 0x14;
                    _mcCopyFileName(0);
                    for (i = 0; i < 0x14; i++) {
                        if (*name == *src) {
                            matchCount--;
                        }
                        *name = *src;
                        name++;
                        src++;
                    }
                    *name = 0;
                    if (matchCount != 0) {
                        // A different file transfers every slot, and the next listing starts at slot 0.
                        work->slotWriteMask = MEMORY_CARD_SLOT_WRITE_ALL;
                    }
                    work->promptId = MEMORY_CARD_PROMPT_CHECKING;
                    task->state    = 5;
                } else {
                    _mcCopyFileName(0);
                    Mc_BuildFileName(Mc_FileName, obj->resultValue);
                    work->promptId = MEMORY_CARD_PROMPT_CHECKING;
                    task->state    = 5;
                }
            } else {
                task->state = 0x29;
            }
            return;
        }
    }

    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    if (syncResult != -1) {
        if (syncResult == 1 && work->syncResult != 0) {
            task->state = 2;
            _mcCloseChildUi(task, syncResult);
        }
    } else {
        MemCardExist(work->channel);
    }
}

/// Return the save-section write mask by comparing each live record with its backup.
///
/// Bit n selects `Mc_BufferSlots[n]`. Compare sections 1..8 byte for byte and
/// always select the card file header (0), saved state (1) and nibble bank (8).
static inline s32 _mcGetSectionWriteMask(void)
{
    enum {
        MEMORY_CARD_SECTION_WRITE_FILE_HEADER = 1 << 0,
        MEMORY_CARD_SECTION_WRITE_SAVE_STATE  = 1 << 1,
        MEMORY_CARD_SECTION_WRITE_NIBBLE_BANK = 1 << 8
    };

    const _McSaveSection* sections;
    const u8*             liveByte;
    const u8*             backupByte;
    u32                   bytesPerCopy;
    u32                   reverseIndex;
    u32                   byteIndex;
    s32                   writeMask;

    writeMask    = 0;
    reverseIndex = 0;
    sections     = Mc_BufferSlots;
    do {
        liveByte     = sections[ARRAY_SIZE(Mc_BufferSlots) - 1 - reverseIndex].buffer;
        bytesPerCopy = sections[ARRAY_SIZE(Mc_BufferSlots) - 1 - reverseIndex].bytesPerCopy;
        byteIndex    = 0;
        backupByte   = liveByte + bytesPerCopy;
        while (byteIndex < bytesPerCopy) {
            if (*liveByte != *backupByte) {
                writeMask |= 1;
            }
            byteIndex  += 1;
            liveByte   += 1;
            backupByte += 1;
        }
        reverseIndex += 1;
        writeMask   <<= 1;
    } while (reverseIndex < (u32)(ARRAY_SIZE(Mc_BufferSlots) - 1));
    writeMask |= MEMORY_CARD_SECTION_WRITE_NIBBLE_BANK;
    writeMask |= MEMORY_CARD_SECTION_WRITE_FILE_HEADER | MEMORY_CARD_SECTION_WRITE_SAVE_STATE;
    return writeMask;
}

static void Mc_StateCompareBuffers(Task* task, McWork* work)
{
    s32           flags;
    u32           textColorRgb;
    u32           status;
    s32           idx;
    s32           one;
    s32           ch;
    s32           i;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = work->syncResult;
    switch (status) {
        case 0:
            work->slotsRemaining   = MEMORY_CARD_BUFFER_SLOT_COUNT;
            flags                  = _mcGetSectionWriteMask();
            work->confirmOverwrite = MEMORY_CARD_OVERWRITE_CONFIRM;
            work->slotWriteMask    = flags;
            task->state            = 0x1F;
            break;
        case 3:
            ptr1 = Mc_FileName;
            ptr0 = Mc_FileNameBuf;
            i    = 0;
            ch   = 0x5F;
            do {
                if (i >= 0xC) {
                    *ptr0 = ch;
                    *ptr1 = ch;
                }
                ptr1++;
                i++;
                ptr0++;
            } while (i < 0x14);
            *ptr0                  = 0;
            *ptr1                  = 0;
            work->slotsRemaining   = MEMORY_CARD_BUFFER_SLOT_COUNT;
            work->slotWriteMask    = MEMORY_CARD_SLOT_WRITE_ALL;
            work->confirmOverwrite = MEMORY_CARD_OVERWRITE_CONFIRM;
            task->state            = 0x1F;
            break;
        case 1:
            task->state = 0x14;
            break;
        case 4:
            task->state = 0x15;
            break;
        case 2:
        default:
            task->state = 0x18;
            break;
    }

    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, one, TEXT_ALIGNMENT_LEFT);
}

/// Open the named save file for writing when the card settle timer reaches zero.
///
/// Requires a positive `cardTimer` and a valid filename. Existing files proceed
/// to overwrite confirmation; missing files to creation; other SDK errors to
/// the access-failed prompt. Keeps drawing the current prompt while waiting.
static void _mcStateOpenSaveFileForWrite(Task* task, McWork* work)
{
    u32 openResult;

    work->cardTimer -= 1;
    if (work->cardTimer == 0) {
        MemCardClose();
        openResult       = MemCardOpen(work->channel, (char*)Mc_FileName, O_WRONLY);
        work->syncResult = openResult;
        switch (openResult) {
            case McErrNone:
                task->state = MEMORY_CARD_SAVE_STATE_CONFIRM_OVERWRITE;
                break;
            case McErrCardNotExist:
                task->state = MEMORY_CARD_SAVE_STATE_ACCESS_FAILED;
                break;
            case McErrCardInvalid:
                task->state = MEMORY_CARD_SAVE_STATE_ACCESS_FAILED;
                break;
            case McErrNewCard:
                task->state = MEMORY_CARD_SAVE_STATE_ACCESS_FAILED;
                break;
            case McErrNotFormat:
                task->state = MEMORY_CARD_SAVE_STATE_ACCESS_FAILED;
                break;
            case McErrFileNotExist:
                task->state = MEMORY_CARD_SAVE_STATE_CONFIRM_CREATE;
                break;
            default:
                task->state = MEMORY_CARD_SAVE_STATE_ACCESS_FAILED;
                break;
        }
    }

    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateCreateFile(Task* task, McWork* work)
{
    u32           textColorRgb;
    u32           status;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->cardTimer -= 1;
    if (work->cardTimer == 0) {
        status           = MemCardCreateFile(work->channel, Mc_FileName, 1);
        work->syncResult = status;
        switch (status) {
            case 0:
                task->state = 0xA;
                break;
            case 1:
                task->state = 0x14;
                break;
            case 4:
                task->state = 0x15;
                break;
            case 7:
                task->state = 0x19;
                break;
            case 2:
            case 3:
            case 5:
            case 6:
            default:
                task->state = 0x2A;
                break;
        }
    }

    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

static void Mc_StatePadFileName(Task* task, McWork* work)
{
    u32           textColorRgb;
    u32           status;
    s32           idx;
    s32           i;
    s32           ch;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = work->syncResult;
    if (status < 4U) {
        ptr1 = Mc_FileName;
        if (status == 0) {
            work->slotsRemaining   = MEMORY_CARD_BUFFER_SLOT_COUNT;
            work->slotWriteMask    = MEMORY_CARD_SLOT_WRITE_ALL;
            work->confirmOverwrite = MEMORY_CARD_OVERWRITE_PROCEED;
            task->state            = 5;
        } else {
            goto pad;
        }
    } else {
        ptr1 = Mc_FileName;
    pad:
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0       = 0;
        *ptr1       = 0;
        task->state = 0x2A;
    }
    work->buffer = 0;

    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

static void Mc_StateNameEntry(Task* task, McWork* work)
{
    s32 syncResult;
    u8* src;
    u8* dst;
    s32 i;

    if (work->confirmOverwrite == MEMORY_CARD_OVERWRITE_CONFIRM) {
        work->promptId = MEMORY_CARD_PROMPT_OVERWRITE;
        switch (Mc_PromptDialogSpawn(task, MEMORY_CARD_PROMPT_OVERWRITE, work->promptTimer)) {
            case 0:
                break;
            case 1:
                work->cardTimer    = MEMORY_CARD_IO_SETTLE_FRAMES;
                work->sectorOffset = 0;
                task->state        = 0x28;
                break;
            case -1:
                src = Mc_FileNameBuf;
                dst = Mc_FileName;
                for (i = 0; i < 0x15; i++) {
                    *dst++ = *src++;
                }
                task->killCountdown = 0xC;
                task->state         = 0x27;
                break;
        }
        syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
        if (syncResult != -1) {
            if (syncResult == 1 && work->syncResult != 0) {
                task->state = 2;
                _mcCloseChildUi(task, syncResult);
            }
        } else {
            MemCardExist(work->channel);
        }
    } else {
        work->sectorOffset = 0;
        work->promptId     = MEMORY_CARD_PROMPT_SAVING;
        task->state        = 0xF;
        _mcDrawPrompt(task, work->promptId);
    }
}

/// Copy every live save record over its adjacent resident backup.
///
/// Sections 1..8 each own two consecutive `bytesPerCopy`-byte records. The card
/// file header in section 0 is excluded because its halves are distinct data.
static inline void _mcBackupSaveSections(void)
{
    _McSaveSection* section;
    _McSaveSection* sections;
    u8*             liveByte;
    u8*             backupByte;
    u32             bytesPerCopy;
    u32             sectionIndex;
    u32             byteIndex;

    sectionIndex = 1;
    sections     = Mc_BufferSlots;
    section      = sections + 1;
    do {
        liveByte     = section->buffer;
        bytesPerCopy = section->bytesPerCopy;
        byteIndex    = 0;
        backupByte   = liveByte + bytesPerCopy;
        while (byteIndex < bytesPerCopy) {
            byteIndex    += 1;
            *backupByte++ = *liveByte++;
        }
        sectionIndex += 1;
        section      += 1;
    } while (sectionIndex < (u32)ARRAY_SIZE(Mc_BufferSlots));
}

/// Store the sum of the live sections' low checksum bytes and its complement.
///
/// Adds the unsigned low byte of each section 1..8 checksum, a sum in 0..2040,
/// into the live save's `bufferChecksum` pair. The card file header is excluded.
static inline void _mcWriteSectionChecksumSummary(void)
{
    _McChecksumBlock* record;
    _McSaveSection*   section;
    _McSaveSection*   sections;
    s16               sum;
    u32               sectionIndex;

    sum          = 0;
    sectionIndex = 1;
    sections     = Mc_BufferSlots;
    section      = sections + 1;
    do {
        record        = section->buffer;
        section      += 1;
        sectionIndex += 1;
        sum          += (u8)record->checksum;
    } while (sectionIndex < (u32)ARRAY_SIZE(Mc_BufferSlots));
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksum           = sum;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksumComplement = ~sum;
}

static void Mc_StateBackupBuffers(Task* task, McWork* work)
{
    u8*   buf;
    s32   size;
    void* mem;

    if (work->slotsRemaining == 0) {
        _mcBackupSaveSections();
        work->closeAnswer = USER_INTERFACE_LIST_COMMAND_YES;
        task->state       = 0x13;
    } else if (work->slotWriteMask & 1) {
        size                = Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - work->slotsRemaining].bytesPerCopy;
        buf                 = Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - work->slotsRemaining].buffer;
        work->transferBytes = (((u32)(size * 2 - 1) >> 7) + 1) << 7;
        mem                 = memCalloc(work->transferBytes, 0);
        if (mem != 0) {
            work->buffer = mem;
            if (work->slotsRemaining == MEMORY_CARD_BUFFER_SLOT_COUNT) {
                // Slot 0 is the card title. The next slot is the save image.
                Mc_BuildSaveTitle(work);
                memcpy(mem, buf, size * 2);
            } else {
                _mcWriteBlockChecksum(buf, size);
                if (work->slotsRemaining == MEMORY_CARD_BUFFER_SLOT_COUNT - 1) {
                    _mcWriteSectionChecksumSummary();
                }
                memcpy(mem, buf, size);
                memcpy((u8*)mem + size, buf, size);
            }
            work->cardTimer      = 0;
            task->state          = task->state + 1;
            work->slotsRemaining = work->slotsRemaining - 1;
            work->slotWriteMask  = (u32)work->slotWriteMask >> 1;
        } else {
            work->cardTimer = work->cardTimer + 1;
        }
    } else {
        work->sectorOffset   = work->sectorOffset + Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - work->slotsRemaining].cardSectors;
        work->slotsRemaining = work->slotsRemaining - 1;
        work->slotWriteMask  = (u32)work->slotWriteMask >> 1;
    }

    work->promptId = MEMORY_CARD_PROMPT_SAVING;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_SAVING);
}

/// Invalidate both remembered card filenames while retaining their product prefix.
///
/// Writes underscores to bytes 12..19 and a terminator to byte 20. Each array
/// must hold at least 21 bytes; the remaining bytes are kept unchanged.
static inline void _mcInvalidateFileNameSuffixes(void)
{
    enum {
        MEMORY_CARD_FILENAME_PREFIX_BYTES = 0xC,
        MEMORY_CARD_FILENAME_BYTES        = 0x14,
        MEMORY_CARD_FILENAME_UNUSED_CHAR  = '_',
    };

    u8* filenameByte;
    u8* savedFilenameByte;
    s32 filenameByteIndex;
    s32 unusedChar;

    filenameByte      = Mc_FileName;
    savedFilenameByte = Mc_FileNameBuf;
    filenameByteIndex = 0;
    unusedChar        = MEMORY_CARD_FILENAME_UNUSED_CHAR;
    do {
        if (filenameByteIndex >= MEMORY_CARD_FILENAME_PREFIX_BYTES) {
            *savedFilenameByte = unusedChar;
            *filenameByte      = unusedChar;
        }
        filenameByte++;
        filenameByteIndex++;
        savedFilenameByte++;
    } while (filenameByteIndex < MEMORY_CARD_FILENAME_BYTES);
    *savedFilenameByte = 0;
    *filenameByte      = 0;
}

/// Interpret a completed section write, choose the next save state and free its buffer.
///
/// Preparation has decremented `slotsRemaining`; success advances the sector
/// offset past that section and resumes the save walk. No card closes the file
/// and shows the insert-card prompt. A changed card invalidates both filename
/// suffixes, closes the file and retries card acceptance. Other errors show save
/// failure. Every path releases and clears the transfer allocation.
static void _mcStateFinishSectionWrite(Task* task, McWork* work)
{
    u32 writeResult;

    writeResult = work->syncResult;
    switch (writeResult) {
        case McErrNone:
            work->sectorOffset += Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - 1 - work->slotsRemaining].cardSectors;
            task->state         = MEMORY_CARD_SAVE_STATE_PREPARE_SECTION;
            break;
        case McErrCardNotExist:
            MemCardClose();
            task->state = MEMORY_CARD_SAVE_STATE_NO_CARD;
            break;
        case McErrNewCard:
            _mcInvalidateFileNameSuffixes();
            MemCardClose();
            task->state = MEMORY_CARD_SAVE_STATE_ACCEPT_CARD;
            break;
        case McErrCardInvalid:
        case McErrNotFormat:
        case McErrFileNotExist:
        default:
            task->state = MEMORY_CARD_SAVE_STATE_WRITE_FAILED;
            break;
    }
    memFree(work->buffer);
    work->buffer = NULL;

    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateFormat(Task* task, McWork* work)
{
    u32           textColorRgb;
    s32           status;
    s32           idx;
    s32           next;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->cardTimer -= 1;
    if (work->cardTimer == 0) {
        status           = MemCardFormat(work->channel);
        work->syncResult = status;
        if (status != 1) {
            if (status != 0) {
                next = 0x2B;
            } else {
                Mc_BuildFileName(Mc_FileName, 0);
                next             = 0x8;
                work->entryCount = 0;
            }
        } else {
            next = 0x14;
        }
        task->state = next;
    }

    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

static void Mc_StateSyncFileSelect(Task* task, McWork* work)
{
    UiObject* obj;
    s32       syncResult;
    s32       i;
    Task*     child;
    UiObject* childObj;
    u8*       src;
    u8*       dst;

    obj            = task->spawnArg2.pointer;
    work->promptId = MEMORY_CARD_PROMPT_SELECT;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_SELECT);

    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    if (syncResult == -1) {
        MemCardExist(work->channel);
    } else if (syncResult == 1 && work->syncResult != 0) {
        task->state = 7;
        _mcCloseChildUi(task, syncResult);
        return;
    }
    child = task->firstChild;
    if (child == NULL) {
        if (Ui_SpawnFromDesc(Mc_SaveListDesc, work, 1, 2, obj) != 0) {
            Mc_SaveSlotList.itemCount = work->entryCount;
            obj->resultValue          = 0;
            obj->panel.control.word   = USER_INTERFACE_PANEL_INACTIVE;
        }
    } else {
        childObj = child->spawnArg2.pointer;
        if (childObj->result == USER_INTERFACE_RESULT_CONFIRM) {
            obj->resultValue             = childObj->resultValue;
            childObj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            uiStartTreeClosing(childObj, childObj->owner);
            obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            if (obj->resultValue >= 0) {
                src = (u8*)work->directory[obj->resultValue].name;
                dst = Mc_FileName;
                for (i = 0; i < 0x14; i++) {
                    *dst++ = *src++;
                }
                *dst        = 0;
                task->state = 0xC;
            } else {
                task->state = 3;
            }
        }
    }
}

/// Jump table of 26 _McStateFunc handlers used by Mc_DispatchStateTable26.
static const _McFileSelectStateTable Mc_FileSelectStates = { {
    Mc_StateInitWorkDefaults,
    Mc_StateSetOpenDefaults,
    Mc_StateCountdownPrompt,
    Mc_StateCloseReturn,
    Mc_StatePromptTimeout,
    Mc_KillIfCountdownAlt,
    Mc_StateEnterPromptF,
    Mc_StateAccept,
    _mcStatePollCardAdvance,
    Mc_StateBlankFileName,
    Mc_StateSyncPrompt3,
    Mc_StateSyncPromptA,
    Mc_StateDrawCurrentPrompt,
    _mcStateOpenSaveFileForRead,
    Mc_StateVerifyFinish,
    _mcStateReadSection,
    _mcStatePollCardAdvance,
    _mcStateFinishSectionRead,
    Mc_StateDrawPrompt1,
    Mc_StateGetDirentry,
    Mc_StateOpenDirEntry,
    Mc_StateReadSlot,
    _mcStatePollCardAdvance,
    _mcStateAdvanceLoadPreview,
    Mc_StateSyncFileSelect,
    Mc_StateEnterPrompt17,
} };

static void Mc_StateBlankFileName(Task* task, McWork* work)
{
    u32           textColorRgb;
    u32           status;
    s32           idx;
    s32           i;
    s32           ch;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = work->syncResult;
    switch (status) {
        case 0:
        case 3:
            ptr1 = Mc_FileName;
            ptr0 = Mc_FileNameBuf;
            i    = 0;
            ch   = 0x5F;
            do {
                if (i >= 0xC) {
                    *ptr0 = ch;
                    *ptr1 = ch;
                }
                ptr1++;
                i++;
                ptr0++;
            } while (i < 0x14);
            *ptr0       = 0;
            *ptr1       = 0;
            task->state = 0x12;
            break;
        case 1:
            task->state = 0xA;
            break;
        case 4:
            task->state = 0xB;
            break;
        case 2:
        default:
            task->state = 0x6;
            break;
    }

    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

/// Wait for the card probe and settle timer, then open the chosen save for loading.
///
/// Idle SDK polling starts a presence probe. Only a completed operation steps
/// the positive timer toward zero. On a successful open, starts the section
/// walk at sector zero; a missing file shows the no-data prompt, other errors
/// the load-failed prompt. A probe error still steps the timer and may open.
static void _mcStateOpenSaveFileForRead(Task* task, McWork* work)
{
    enum { MEMORY_CARD_SYNC_IDLE     = -1,
           MEMORY_CARD_SYNC_COMPLETE = 1,
           MEMORY_CARD_SYNC_POLL     = 1 };

    s32   syncState;
    u32   openResult;
    char* fileName;

    _mcDrawPrompt(task, work->promptId);

    syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &work->syncCommand, &work->syncResult);
    if (syncState != MEMORY_CARD_SYNC_IDLE) {
        if (syncState == MEMORY_CARD_SYNC_COMPLETE) {
            if (work->syncCommand == McFuncExist) {
                if (work->syncResult != McErrNone) {
                    task->state = MEMORY_CARD_LOAD_STATE_FAILED;
                }
            }
            work->cardTimer -= 1;
            if (work->cardTimer == 0) {
                fileName = (char*)Mc_FileName;
                MemCardClose();
                openResult       = MemCardOpen(work->channel, fileName, O_RDONLY);
                work->syncResult = openResult;
                switch (openResult) {
                    case McErrNone:
                        work->sectorOffset = 0;
                        task->state        = MEMORY_CARD_LOAD_STATE_PREPARE_SECTION;
                        break;
                    case McErrCardNotExist:
                    case McErrCardInvalid:
                        task->state = MEMORY_CARD_LOAD_STATE_FAILED;
                        break;
                    case McErrFileNotExist:
                        task->state = MEMORY_CARD_LOAD_STATE_NO_DATA;
                        break;
                    case McErrNewCard:
                    case McErrNotFormat:
                    default:
                        task->state = MEMORY_CARD_LOAD_STATE_FAILED;
                        break;
                }
            }
        }
    } else {
        MemCardExist(work->channel);
    }
}

/// Return whether every live save section carries its signed-byte payload sum.
///
/// Sections 1..8 cover `bytesPerCopy` minus the four-byte record header. Each sum
/// retains its low 16 bits; backup records and checksum complements are not
/// checked. All live sections are visited even after a mismatch.
static inline s32 _mcVerifySaveSectionChecksums(void)
{
    const _McChecksumBlock* record;
    const _McSaveSection*   section;
    const _McSaveSection*   sections;
    s16                     sum;
    u32                     payloadByteCount;
    u32                     sectionIndex;
    u32                     byteIndex;
    const u8*               payloadByte;
    s32                     allMatch;

    allMatch     = 1;
    sectionIndex = 1;
    sections     = Mc_BufferSlots;
    section      = sections + 1;
    do {
        sum              = 0;
        record           = section->buffer;
        payloadByteCount = section->bytesPerCopy;
        payloadByte      = record->payload;
        payloadByteCount = payloadByteCount - sizeof(_McChecksumBlock);
        byteIndex        = 0;
        while (byteIndex < payloadByteCount) {
            byteIndex += 1;
            sum       += (s8)*payloadByte++;
        }
        if (record->checksum != (sum & MEMORY_CARD_CHECKSUM_MASK)) {
            allMatch = 0;
        }
        sectionIndex += 1;
        section      += 1;
    } while (sectionIndex < (u32)ARRAY_SIZE(Mc_BufferSlots));
    return allMatch;
}

/// Write the signed-byte payload checksum pair of every live save section.
///
/// Sections 1..8 cover `bytesPerCopy` minus the four-byte record header. Each sum
/// retains its low 16 bits. The card file header and resident backups are left
/// alone; the complement is stored before the checksum.
static inline void _mcWriteSaveSectionChecksums(void)
{
    _McChecksumBlock* record;
    _McSaveSection*   section;
    _McSaveSection*   sections;
    s16               sum;
    s32               checksumMask;
    u32               payloadByteCount;
    u32               sectionIndex;
    u32               byteIndex;
    u8*               payloadByte;

    sectionIndex = 1;
    checksumMask = MEMORY_CARD_CHECKSUM_MASK;
    sections     = Mc_BufferSlots;
    section      = sections + 1;
    do {
        sum              = 0;
        byteIndex        = 0;
        record           = section->buffer;
        payloadByteCount = section->bytesPerCopy;
        payloadByte      = record->payload;
        payloadByteCount = payloadByteCount - sizeof(_McChecksumBlock);
        while (byteIndex < payloadByteCount) {
            byteIndex += 1;
            sum       += (s8)*payloadByte++;
        }
        section                   += 1;
        sectionIndex              += 1;
        record->checksumComplement = checksumMask - sum;
        record->checksum           = sum;
    } while (sectionIndex < (u32)ARRAY_SIZE(Mc_BufferSlots));
}

/// Return whether the live save has the sum of sections 1..8's low checksum bytes.
///
/// The bytes are unsigned, giving a sum in 0..2040. Checks `bufferChecksum` only;
/// its complement, resident backups and the card file header are not checked.
static inline s32 _mcVerifySectionChecksumSummary(void)
{
    const _McChecksumBlock* record;
    s32                     sum;
    u32                     sectionIndex;
    const _McSaveSection*   section;
    const _McSaveSection*   sections;

    sum          = 0;
    sectionIndex = 1;
    sections     = Mc_BufferSlots;
    section      = sections + 1;
    do {
        record        = section->buffer;
        sum          += (u8)record->checksum;
        section      += 1;
        sectionIndex += 1;
    } while (sectionIndex < (u32)ARRAY_SIZE(Mc_BufferSlots));
    return (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksum ^ (sum & MEMORY_CARD_CHECKSUM_MASK)) == 0;
}

static void Mc_StateVerifyFinish(Task* task, McWork* work)
{
    s32   size;
    void* mem;

    if (work->slotsRemaining == 0) {
        if (_mcVerifySaveSectionChecksums() && _mcVerifySectionChecksumSummary()) {
            playClockResetMinuteTicks();
            gDisplayState.control.flags.pendingPlayerPos = 1;
            task->state                                  = 3;
        } else {
            mcResetSaveData();
            task->state = 0x19;
        }
    } else if (work->slotWriteMask & 1) {
        size                = Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - work->slotsRemaining].bytesPerCopy;
        size              <<= 1;
        size               -= 1;
        size                = (u32)size >> 7;
        size               += 1;
        size              <<= 7;
        work->transferBytes = size;
        mem                 = memMalloc(size, false);
        work->buffer        = mem;
        if (mem != 0) {
            work->cardTimer      = 0;
            task->state          = task->state + 1;
            work->slotsRemaining = work->slotsRemaining - 1;
            work->slotWriteMask  = (u32)work->slotWriteMask >> 1;
        } else {
            work->cardTimer = work->cardTimer + 1;
        }
    } else {
        work->sectorOffset   = work->sectorOffset + Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - work->slotsRemaining].cardSectors;
        work->slotsRemaining = work->slotsRemaining - 1;
        work->slotWriteMask  = (u32)work->slotWriteMask >> 1;
    }

    work->promptId = MEMORY_CARD_PROMPT_LOADING;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_LOADING);
}

/// Store the read card file header's signed-byte sum and complement in dialog work.
///
/// `work->buffer` must borrow the complete card file header for this call. Covers
/// the same byte extent as `Mc_DefaultChecksumSrc`, retaining the low 16 bits;
/// the stored sum can be compared with the live save's title checksum.
static inline void _mcWriteReadCardHeaderChecksum(McWork* work)
{
    s16       sum;
    s32       headerByteCount;
    const u8* headerByte;
    u16*      checksum;
    s32       byteIndex;

    sum                                                       = 0;
    headerByteCount                                           = sizeof(Mc_DefaultChecksumSrc);
    headerByte                                                = work->buffer;
    checksum                                                  = &work->checksum;
    byteIndex                                                 = 0;
    *checksum                                                 = 0;
    PARENT_OF(checksum, McWork, checksum)->checksumComplement = ~0;
    do {
        byteIndex  += 1;
        sum        += (s8)*headerByte;
        headerByte += 1;
    } while (byteIndex < headerByteCount);
    *checksum                                                 = sum;
    PARENT_OF(checksum, McWork, checksum)->checksumComplement = ~sum;
}

/// Consume a completed section read, advance the load walk and release its buffer.
///
/// `slotsRemaining` has already been decremented by preparation, so
/// `8 - slotsRemaining` must index sections 0..8. Section zero computes the
/// file-header checksum in dialog work; other sections copy both live and
/// backup records into resident storage. Sector rounding bytes are not copied.
/// Any SDK error invalidates both filename suffixes and shows load failure;
/// the transfer allocation is freed and cleared on every path.
static void _mcStateFinishSectionRead(Task* task, McWork* work)
{
    enum { MEMORY_CARD_FILE_HEADER_SECTION = 0 };

    u32 readResult;
    s32 sectionIndex;
    s32 recordPairBytes;

    readResult = work->syncResult;
    if (readResult < (u32)McErrNotFormat) {
        if (readResult == McErrNone) {
            // The file header is checksummed; other sections restore both resident copies.
            sectionIndex = MEMORY_CARD_BUFFER_SLOT_COUNT - 1 - work->slotsRemaining;
            if (sectionIndex == MEMORY_CARD_FILE_HEADER_SECTION) {
                _mcWriteReadCardHeaderChecksum(work);
            } else {
                recordPairBytes   = Mc_BufferSlots[sectionIndex].bytesPerCopy;
                recordPairBytes <<= 1;
                memcpy(Mc_BufferSlots[sectionIndex].buffer, work->buffer, recordPairBytes);
            }
            work->sectorOffset += Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - 1 - work->slotsRemaining].cardSectors;
            task->state         = MEMORY_CARD_LOAD_STATE_PREPARE_SECTION;
        } else {
            goto invalidateFilename;
        }
    } else {
    invalidateFilename:
        _mcInvalidateFileNameSuffixes();
        task->state = MEMORY_CARD_LOAD_STATE_FAILED;
    }

    memFree(work->buffer);
    work->buffer = NULL;
    _mcDrawPrompt(task, work->promptId);
}

/// Return whether a save preview names a place in 1..16 and has a valid header sum.
///
/// Covers 56 signed bytes from `location`, including the checksum pair and
/// retained preview bytes; compares the low 16 bits with `headerChecksum`.
/// The complement is included in the byte sum, without a separate pair check.
/// `preview` must borrow at least one complete `McSavePreview`.
static inline s32 _mcVerifySavePreviewChecksum(const McSavePreview* preview)
{
    u16       sum;
    const u8* headerByte;
    s32       checksumByteCount;
    s32       byteIndex;

    sum = 0;
    if ((u32)(preview->savePoint - 1) >= (u32)MEMORY_CARD_SAVE_POINT_COUNT) {
        return 0;
    }
    // The serialized checksum spans fields and retained bytes beyond the location cell.
    headerByte        = (const u8*)preview + OFFSET_OF(McSavePreview, location);
    checksumByteCount = MEMORY_CARD_SAVE_HEADER_CHECKSUM_BYTES;
    byteIndex         = 0;
    do {
        byteIndex  += 1;
        sum        += (s8)*headerByte;
        headerByte += 1;
    } while (byteIndex < checksumByteCount);
    return preview->headerChecksum == sum;
}

static void Mc_StateSaveSlotUi(UiList* list, UiObject* object)
{
    McWork* work;
    s32     previewByteOffset;
    s32     enabled;

    enabled = 1;
    // Scale the index and add the preview offset before the work base.
    previewByteOffset = list->currentItemIndex * sizeof(McSavePreview) + OFFSET_OF(McWork, previews);
    work              = object->owner->spawnArg1.pointer;
    if (!_mcVerifySavePreviewChecksum((McSavePreview*)((u8*)work + previewByteOffset))) {
        enabled = 0;
        uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
    }
    Mc_DrawSlotDetails(object, work, list->currentItemIndex, 0, list->rowTextY.signedValue + 7);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (enabled && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm)) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
            object->resultValue = list->currentItemIndex;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel)) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
            object->resultValue = -1;
        }
    }
}

void Mc_DrawSlotDetails(UiObject* object, McWork* work, s32 slot, s32 arg3, s32 arg4)
{
    union {
        u8          buf[0x20];
        TextDrawReq req;
    } sp20;
    TextDrawReq timeLabelRequest;
    TextDrawReq timeValueRequest;
    union {
        u8          buf[0x10];
        TextDrawReq req;
    } sp60;
    TextDrawReq    detailRequest;
    TextDrawReq    statRequest;
    TextDrawReq    bpValueRequest;
    s32            x;
    s32            y;
    s32            textX;
    u32            textColorRgb;
    McSavePreview* save;

    textColorRgb = uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_NORMAL);
    if (slot < work->entryCount) {
        save = &work->previews[slot];
        if (!_mcVerifySavePreviewChecksum(save)) {
            x = arg3 + object->panel.contentLeft.signedValue + 8;
            y = arg4 + object->panel.contentTop.signedValue + 0x11;
            if (work->corruptNoticeStyle == MEMORY_CARD_CORRUPT_NOTICE_PROMPT) {
                textDrawUiLine(object, x, y, McText_LoadAbortedCorrupted, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
                return;
            }
            textDrawUiLines(object, x, y, McText_LoadAbortedCorrupted, 0x37A78, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
            return;
        }
        x                           = arg3 + object->panel.contentLeft.signedValue + 2;
        y                           = (arg4 + object->panel.contentBottom.signedValue) - 0x10;
        timeLabelRequest.x          = object->panel.contentOriginX.unsignedValue + x;
        timeLabelRequest.y          = object->panel.contentOriginY.unsignedValue + (y - 2);
        timeLabelRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        timeLabelRequest.colorRgb   = 0x606060;
        timeLabelRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        timeLabelRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        timeLabelRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&timeLabelRequest, McText_Time);
        timeValueRequest.x          = object->panel.contentOriginX.unsignedValue + 0x28 + x;
        timeValueRequest.y          = object->panel.contentOriginY.unsignedValue + y;
        timeValueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        timeValueRequest.colorRgb   = textColorRgb;
        timeValueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        timeValueRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        timeValueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&timeValueRequest, textFormatPlayTime(sp20.buf, save->playTime));
        if (save->clearCount > 0) {
            x                   = (arg3 + object->panel.contentRight.signedValue) - 4;
            y                   = (arg4 + object->panel.contentBottom.signedValue) - 0xB;
            sp60.req.x          = object->panel.contentOriginX.unsignedValue + (x - 0x1E);
            sp60.req.y          = object->panel.contentOriginY.unsignedValue + (y - 2);
            sp60.req.otIndex    = object->panel.otIndex.signedValue + 1;
            sp60.req.colorRgb   = 0x606060;
            sp60.req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            sp60.req.alignment  = TEXT_ALIGNMENT_RIGHT;
            sp60.req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&sp60.req, McText_Clear);
            detailRequest.x          = object->panel.contentOriginX.unsignedValue + x;
            detailRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            detailRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            detailRequest.colorRgb   = textColorRgb;
            detailRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            detailRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
            detailRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&detailRequest, textItoaUnsigned(sp20.buf, save->clearCount));
            if ((s8)save->savePoint != MEMORY_CARD_SAVE_POINT_OPENING) {
                statRequest.x          = object->panel.contentOriginX.unsignedValue + x;
                statRequest.y          = object->panel.contentOriginY.unsignedValue + 8 + y;
                statRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                statRequest.colorRgb   = 0x606060;
                statRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                statRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
                statRequest.drawMode   = TEXT_DRAW_OUTLINED;
                textDrawString(&statRequest, Mc_ModeLabels[save->gameMode]);
            }
        }
        x = arg3 + object->panel.contentLeft.signedValue + 4;
        y = arg4 + object->panel.contentTop.signedValue + 0x11;
        textDrawUiLine(object, x, y, Mc_LocationLabels[(s8)save->savePoint], textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        sp60.buf[0] = 0;
        textAppendString(sp60.buf, McText_OpenParen);
        textAppendString(sp60.buf, textItoaSigned(sp20.buf, save->saveNumber));
        textAppendString(sp60.buf, McText_CloseParen);
        detailRequest.x          = object->panel.contentOriginX.unsignedValue + (x + textMeasureLineWidth(Mc_LocationLabels[(s8)save->savePoint]));
        detailRequest.y          = object->panel.contentOriginY.unsignedValue + (y - 3);
        detailRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        detailRequest.colorRgb   = textColorRgb;
        detailRequest.glyphTable = TEXT_GLYPH_TABLE_LARGE;
        detailRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        detailRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&detailRequest, sp60.buf);
        textX                    = arg3 + object->panel.contentLeft.signedValue;
        x                        = textX + 2;
        y                        = (arg4 + object->panel.contentBottom.signedValue) - 1;
        detailRequest.x          = object->panel.contentOriginX.unsignedValue + x;
        x                       += 0x28;
        detailRequest.y          = object->panel.contentOriginY.unsignedValue + (y - 2);
        detailRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        detailRequest.colorRgb   = 0x606060;
        detailRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        detailRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        detailRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&detailRequest, McText_Exp);
        if ((s8)save->savePoint != MEMORY_CARD_SAVE_POINT_OPENING) {
            statRequest.x          = object->panel.contentOriginX.unsignedValue + x;
            statRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            statRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            statRequest.colorRgb   = 0x606060;
            statRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            statRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            statRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&statRequest, textItoaSigned(sp20.buf, save->playerExp));
        } else {
            statRequest.x          = object->panel.contentOriginX.unsignedValue + x;
            statRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            statRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            statRequest.colorRgb   = 0x606060;
            statRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            statRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            statRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&statRequest, McText_Unavailable);
        }
        x                      = arg3 - 0x28;
        statRequest.x          = object->panel.contentOriginX.unsignedValue + x;
        statRequest.y          = object->panel.contentOriginY.unsignedValue + (y - 2);
        statRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        statRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        statRequest.colorRgb   = 0x606060;
        statRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        statRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&statRequest, McText_Bp);
        if ((s8)save->savePoint != MEMORY_CARD_SAVE_POINT_OPENING) {
            bpValueRequest.x          = object->panel.contentOriginX.unsignedValue + 0x1E + x;
            bpValueRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            bpValueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            bpValueRequest.colorRgb   = 0x606060;
            bpValueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            bpValueRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            bpValueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&bpValueRequest, textItoaSigned(sp20.buf, save->playerBp));
        } else {
            bpValueRequest.x          = object->panel.contentOriginX.unsignedValue + 0x1E + x;
            bpValueRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            bpValueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            bpValueRequest.colorRgb   = 0x606060;
            bpValueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            bpValueRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            bpValueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&bpValueRequest, McText_Unavailable);
        }
    } else {

        sp20.req.x          = object->panel.contentOriginX.unsignedValue + arg3;
        sp20.req.y          = object->panel.contentOriginY.unsignedValue + 5 + arg4;
        sp20.req.otIndex    = object->panel.otIndex.signedValue + 1;
        sp20.req.glyphTable = TEXT_GLYPH_TABLE_LARGE;
        sp20.req.colorRgb   = textColorRgb;
        sp20.req.alignment  = TEXT_ALIGNMENT_CENTER;
        sp20.req.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&sp20.req, McText_NewBlock);
    }
}

static u16* Mc_EncodeAsciiGlyphs(s8* arg0, u16* arg1)
{
    s32 ch;
    u16 idx;
    u8  ch_u;

    ch_u = *arg0;
    if (*arg0 != 0) {
        do {
            ch = (s8)ch_u;
            if (ch >= 0x61) {
                idx = Mc_GlyphsLower[ch - 0x61];
                goto store;
            }
            if (ch >= 0x41) {
                idx = Mc_GlyphsUpper[ch - 0x41];
                goto store;
            }
            if (ch >= 0x20) {
                idx = Mc_GlyphsSymbol[ch - 0x20];
            store:
                *arg1 = idx;
            }
            arg0++;
            ch_u = *arg0;
            arg1++;
        } while (*arg0 != 0);
    }
    *arg1 = 0;
    return arg1;
}

static void Mc_InitFileName(void)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    ptr1 = Mc_FileName;
    ptr0 = Mc_FileNameBuf;
    i    = 0;
    ch   = 0x5F;
    do {
        if (i >= 0xC) {
            *ptr0 = ch;
            *ptr1 = ch;
        }
        ptr1++;
        i++;
        ptr0++;
    } while (i < 0x14);
    *ptr0 = 0;
    *ptr1 = 0;
}

static void Mc_CopyFileName(s32 arg0)
{
    _mcCopyFileName(arg0);
}

static void Mc_WriteSaveHdrChecksum(void)
{
    s16 sum;
    u8* ptr;
    s32 limit;
    s32 i;
    s16 tmp;

    sum                                                               = 0;
    ptr                                                               = (u8*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    ptr                                                              += OFFSET_OF(McSavePreview, location);
    limit                                                             = MEMORY_CARD_SAVE_HEADER_CHECKSUM_BYTES;
    i                                                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.headerChecksum           = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.headerChecksumComplement = 0xFFFF;
    do {
        i   += 1;
        tmp  = (s8)*ptr;
        sum  = sum + tmp;
        ptr += 1;
    } while (i < limit);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.headerChecksum           = sum;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.headerChecksumComplement = ~sum;
    _mcVerifySaveHeaderChecksum(&gMcSaveData[MEMORY_CARD_SAVE_LIVE]);
}

/// Check a resident save's preview checksum and its place-label range (1..16).
///
/// Reads the shared prefix through `save->preview`; neither the complete saved
/// state payload nor the card file header is verified. Returns nonzero on success.
static s32 _mcVerifySaveHeaderChecksum(const McSaveData* save)
{
    return _mcVerifySavePreviewChecksum(&save->preview);
}

/// Out-of-line form of `_mcWriteBlockChecksum`. Nothing calls it.
static void Mc_WriteBlockChecksum(u8* data, s32 size)
{
    _McChecksumBlock* block;
    s16               sum;
    u32               i;

    block = (_McChecksumBlock*)data;
    sum   = 0;
    data  = block->payload;
    size -= sizeof(_McChecksumBlock);
    i     = 0;
    if (size != 0) {
        do {
            i    += 1;
            sum  += (s8)*data;
            data += 1;
        } while (i < size);
    }
    block->checksum           = sum;
    block->checksumComplement = ~sum;
}

void mcResetOptions(void)
{
    _mcRestoreOptionDefaults();
}

/// Reset the five stage-flag record pairs without seeding a player or save image.
///
/// Live records are zeroed and backups filled with 0xFF. Does not recompute
/// checksums or reset the nibble bank. Retained unused entry point.
static void _mcResetStageFlagRecords(void)
{
    _mcResetStageFlagCopies();
}

void mcInit(void)
{
    enum { MEMORY_CARD_AUTOMATIC_CONTROL_DISABLED = 0 };

    MemCardInit(MEMORY_CARD_AUTOMATIC_CONTROL_DISABLED);
    MemCardStart();
    mcResetSaveData();
}

/// Whether a buffer's header holds the sum of its payload, as
/// `Mc_WriteBlockChecksum` stores it. Only the sum is compared, not its
/// complement. Nothing calls it.
static s32 Mc_VerifyBlockChecksum(u8* data, s32 size)
{
    _McChecksumBlock* block;
    s16               sum;
    u32               i;

    block = (_McChecksumBlock*)data;
    sum   = 0;
    data  = block->payload;
    size -= sizeof(_McChecksumBlock);
    i     = 0;
    if (size != 0) {
        do {
            i    += 1;
            sum  += (s8)*data;
            data += 1;
        } while (i < size);
    }
    return (block->checksum ^ (sum & 0xFFFF)) == 0;
}

static void Mc_UnusedStub(void)
{
}

static s32 Mc_CompareBufferHalves(void)
{
    return _mcGetSectionWriteMask();
}

static void Mc_WriteSlotChecksums(void)
{
    _mcWriteSaveSectionChecksums();
}

static void Mc_WriteFirstByteChecksum(void)
{
    _mcWriteSectionChecksumSummary();
}

static s32 Mc_VerifyFirstByteChecksum(void)
{
    return _mcVerifySectionChecksumSummary();
}

static s32 Mc_VerifySlotChecksums(void)
{
    return _mcVerifySaveSectionChecksums();
}

static void Mc_DuplicateBuffers(void)
{
    u32             i;
    u32             j;
    _McSaveSection* p;
    _McSaveSection* base;
    u8*             src;
    s32             size;
    u8*             dest;

    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        src  = p->buffer;
        size = p->bytesPerCopy;
        j    = 0;
        dest = src + size;
        while (j < (u32)size) {
            j    += 1;
            *dest = *src;
            src  += 1;
            dest += 1;
        }
        i += 1;
        p += 1;
    } while (i < 9);
}

static void Mc_DrawPrompt(Task* task, s32 arg1)
{
    _mcDrawPrompt(task, arg1);
}

static void Mc_HideChildUi(Task* task)
{
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    child = task->firstChild;
    if (child != NULL) {
        obj                     = child->spawnArg2.pointer;
        flag                    = task->spawnArg2.pointer;
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        uiStartTreeClosing(obj, obj->owner);
        flag->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }
}

static void Mc_WriteDataChecksum(s32 arg0, McWork* work)
{
    s16  sum;
    s32  count;
    u8*  src;
    s16* dst;
    s32  i;

    sum   = 0;
    count = 0x200;
    if (arg0 == 0) {
        src = Mc_DefaultChecksumSrc;
        dst = (s16*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.titleChecksum;
    } else {
        src = work->buffer;
        dst = (s16*)&work->checksum;
    }

    i      = 0;
    dst[0] = sum;
    dst[1] = ~sum;
    while (i < count) {
        i   += 1;
        sum += (s8)*src;
        src += 1;
    }
    dst[0] = sum;
    dst[1] = ~sum;
}

static s32 Mc_CompareSaveChecksum(McSaveData* save, McWork* work)
{
    if (save->state.cheatMode != 0) {
        return 0;
    }
    if (save->state.demoScene != 0) {
        return 0;
    }
    return save->state.titleChecksum == work->checksum;
}

static void Mc_ResetWork(Task* task, McWork* work)
{
    work->promptTimer        = MEMORY_CARD_PROMPT_LEAD_FRAMES;
    work->cardTimer          = 0;
    work->buffer             = 0;
    work->channel            = 0;
    work->closeAnswer        = USER_INTERFACE_LIST_COMMAND_NO;
    work->corruptNoticeStyle = MEMORY_CARD_CORRUPT_NOTICE_PROMPT;
    task->state++;
}

static void Mc_WriteSlotChecksumsEx(Task* task, McWork* work)
{
    work->slotsRemaining   = MEMORY_CARD_BUFFER_SLOT_COUNT;
    work->slotWriteMask    = MEMORY_CARD_SLOT_WRITE_ALL;
    work->confirmOverwrite = MEMORY_CARD_OVERWRITE_CONFIRM;
    _mcWriteSaveSectionChecksums();

    if (task->spawnArg1.value != 0) {
        task->killCountdown = 2;
        task->state         = 0x27;
    } else {
        task->state = 0xE;
    }
}

static void Mc_StateAcceptMode1(Task* task, McWork* work)
{
    u32           textColorRgb;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->promptId = MEMORY_CARD_PROMPT_CHECKING;
    if (MemCardAccept(work->channel) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    work->cardTimer = work->cardTimer + 1;
    idx             = work->promptId;
    obj             = task->spawnArg2.pointer;
    textColorRgb    = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result     = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (work->promptTimer > 0) {
        work->promptTimer -= 2;
    }
    if (work->promptTimer < 0) {
        work->promptTimer += 2;
    }
}

/// Poll card I/O and advance either dialog machine when no operation is pending.
///
/// A zero sync return keeps this state and increments the card wait counter;
/// any nonzero return, including -1, clears it and advances to the next state.
/// The following state interprets `work->syncResult`. Draws the current prompt
/// and steps its signed frame timer toward zero by two; odd positive values stay at 1.
static void _mcStatePollCardAdvance(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_SYNC_POLL                   = 1,
        MEMORY_CARD_PROMPT_APPROACH_STEP_FRAMES = 2
    };

    if (MemCardSync(MEMORY_CARD_SYNC_POLL, &work->syncCommand, &work->syncResult) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    _mcDrawPrompt(task, work->promptId);
    // Test the negative case after the positive step; an odd positive timer stays at 1.
    if (work->promptTimer > 0) {
        work->promptTimer -= MEMORY_CARD_PROMPT_APPROACH_STEP_FRAMES;
    }
    if (work->promptTimer < 0) {
        work->promptTimer += MEMORY_CARD_PROMPT_APPROACH_STEP_FRAMES;
    }
}

static void Mc_StateDrawPromptAdvance(Task* task, McWork* work)
{
    u32           textColorRgb;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->cardTimer = MEMORY_CARD_IO_SETTLE_FRAMES;
    obj             = task->spawnArg2.pointer;
    idx             = work->promptId;
    textColorRgb    = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result     = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->state = task->state + 1;
}

static void Mc_StatePromptChoiceB(Task* task, McWork* work)
{
    s32       ret;
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;
    u8*       src;
    u8*       dst;
    s32       i;

    work->promptId = MEMORY_CARD_PROMPT_CREATE;
    ret            = Mc_PromptDialogChoice(task, MEMORY_CARD_PROMPT_CREATE, work->promptTimer);
    if (ret != -1) {
        if (ret == 1) {
            task->state = 8;
        }
    } else {
        src = Mc_FileNameBuf;
        dst = Mc_FileName;
        for (i = 0; i < 0x15; i++) {
            *dst++ = *src++;
        }
        task->killCountdown = 0xC;
        task->state         = 0x27;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (work->syncResult != 0) {
                child       = task->firstChild;
                task->state = 2;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    uiStartTreeClosing(obj, obj->owner);
                    flag->panel.control.word = syncResult;
                }
            }
        }
    } else {
        MemCardExist(work->channel);
    }
}

static void Mc_StateDrawPrompt4(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->cardTimer = MEMORY_CARD_IO_SETTLE_FRAMES;
    work->promptId  = MEMORY_CARD_PROMPT_SAVING;
    obj             = task->spawnArg2.pointer;
    textColorRgb    = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result     = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[MEMORY_CARD_PROMPT_SAVING];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->state = task->state + 1;
}

static void Mc_StateEnterDialog4(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->cardTimer = 0;
    task->state++;
    work->promptId = MEMORY_CARD_PROMPT_SAVING;
    obj            = task->spawnArg2.pointer;
    textColorRgb   = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[MEMORY_CARD_PROMPT_SAVING];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

/// Submit the complete 512-byte card file header by filename and draw the prompt.
///
/// Retries while the SDK refuses the request, counting refused frames in
/// `cardTimer`. Acceptance advances to the sync state; it does not mean the
/// write has finished. The resident header stays available through completion.
/// The SDK word-pointer view addresses the same word-aligned header bytes.
static void _mcStateWriteFileHeader(Task* task, McWork* work)
{

    if (MemCardWriteFile(work->channel, (char*)Mc_FileName, (unsigned long*)Mc_DefaultChecksumSrc, 0,
                         sizeof(Mc_DefaultChecksumSrc)) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StatePromptChoiceGeneric(Task* task, McWork* work)
{
    s32 ret;

    work->promptId = MEMORY_CARD_PROMPT_SAVE;
    ret            = Mc_PromptDialogChoice(task, MEMORY_CARD_PROMPT_SAVE, work->promptTimer);
    switch (ret) {
        case 0:
            break;
        case 1:
            task->killCountdown = 0xC;
            task->state         = 0x27;
            break;
        case -1:
            task->state = 0x13;
            break;
    }
    if (work->promptTimer > 0) {
        work->promptTimer -= 2;
    }
    if (work->promptTimer < 0) {
        work->promptTimer += 2;
    }
}

/// Submit the prepared section write and draw the current prompt until accepted.
///
/// `buffer` must own at least `transferBytes` bytes; `sectorOffset` counts
/// 128-byte sectors, while the SDK takes a byte offset. Acceptance advances to
/// polling; refusal increments `cardTimer`. The buffer remains owned by the
/// dialog until the completion handler frees it.
static void _mcStateWriteSection(Task* task, McWork* work)
{

    if (MemCardWriteData(work->buffer, work->sectorOffset << MEMORY_CARD_SECTOR_BYTE_SHIFT, work->transferBytes) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateClosePrompt(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;
    UiObject*     flag;
    s16           val;

    MemCardClose();
    idx          = work->promptId;
    obj          = task->spawnArg2.pointer;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->state = 0x1B;
    flag        = task->spawnArg2.pointer;
    if (flag != NULL) {
        val               = work->closeAnswer; // s32 to s16: resultValue is a halfword
        flag->result      = USER_INTERFACE_RESULT_CANCEL;
        flag->resultValue = val;
    }
}

static void Mc_KillIfCountdown(Task* task, McWork* unused2)
{
    if (task->killCountdown != 0) {
        taskKill(task);
    }
}

static void Mc_StateSyncPromptFile3(Task* task, McWork* work)
{
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    work->promptId = MEMORY_CARD_PROMPT_NO_CARD;
    if (Mc_PromptDialogFile(task, MEMORY_CARD_PROMPT_NO_CARD, work->promptTimer) != 0) {
        task->state = 0x13;
        return;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    switch (syncResult) {
        case -1:
            MemCardExist(work->channel);
            return;
        case 1:
            if (work->syncResult != syncResult) {
                child = task->firstChild;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    uiStartTreeClosing(obj, obj->owner);
                    flag->panel.control.word = syncResult;
                }
                task->state = 2;
            }
            return;
        case 0:
            return;
    }
}

static void Mc_StatePromptChoice9(Task* task, McWork* work)
{
    s32       ret;
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    work->promptId = MEMORY_CARD_PROMPT_UNFORMATTED;
    ret            = Mc_PromptDialogSpawn(task, MEMORY_CARD_PROMPT_UNFORMATTED, work->promptTimer);
    switch (ret) {
        case 0:
            break;
        case 1:
            task->state = 0x16;
            break;
        case -1:
            task->killCountdown = 0xC;
            task->state         = 0x29;
            break;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (work->syncResult != 0) {
                child       = task->firstChild;
                task->state = 2;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    uiStartTreeClosing(obj, obj->owner);
                    flag->panel.control.word = syncResult;
                }
            }
        }
    } else {
        MemCardExist(work->channel);
    }
}

static void Mc_StateColdBoot(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->promptId = MEMORY_CARD_PROMPT_FORMATTING;
    obj            = task->spawnArg2.pointer;
    textColorRgb   = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[MEMORY_CARD_PROMPT_FORMATTING];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    work->cardTimer = MEMORY_CARD_IO_SETTLE_FRAMES;
    task->state     = task->state + 1;
}

static void Mc_StateSyncPrompt13(Task* task, McWork* work)
{
    s32       syncResult;
    s32       rslt;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    work->promptId = MEMORY_CARD_PROMPT_CARD_FULL;
    if (Mc_PromptDialogFile(task, MEMORY_CARD_PROMPT_CARD_FULL, work->promptTimer) != 0) {
        task->state = 0x13;
        return;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    switch (syncResult) {
        case -1:
            MemCardExist(work->channel);
            return;
        case 1:
            rslt = work->syncResult;
            if (rslt == syncResult) {
                child = task->firstChild;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    uiStartTreeClosing(obj, obj->owner);
                    flag->panel.control.word = rslt;
                }
                task->state = 0x14;
            }
            return;
        case 0:
            return;
    }
}

static void Mc_StateEnterPrompt0(Task* task, McWork* work)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    work->promptId  = MEMORY_CARD_PROMPT_ACCESS_FAILED;
    work->cardTimer = 0;
    if (Mc_PromptDialog(task, work->promptId, 0) != 0) {
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0       = 0;
        *ptr1       = 0;
        task->state = 0x13;
    }
}

static void Mc_StatePromptCountdown(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    work->promptTimer -= 1;
    obj                = task->spawnArg2.pointer;
    idx                = work->promptId;
    textColorRgb       = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result        = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (work->promptTimer < MEMORY_CARD_PROMPT_DISMISS_LIMIT) {
        task->killCountdown = 0;
        task->state         = -1;
    }
}

static void Mc_StateDrawPromptTo1F(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    idx          = work->promptId;
    obj          = task->spawnArg2.pointer;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->state = 0x1F;
}

static void Mc_StateCountdownPrompt4(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->promptId = MEMORY_CARD_PROMPT_SAVING;
    obj            = task->spawnArg2.pointer;
    textColorRgb   = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[MEMORY_CARD_PROMPT_SAVING];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (work->cardTimer-- <= 0) {
        work->closeAnswer = USER_INTERFACE_LIST_COMMAND_YES;
        task->state       = 0x13;
    }
}

static void Mc_StateDrawPrompt1Advance(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->cardTimer = MEMORY_CARD_IO_BRIEF_FRAMES;
    work->promptId  = MEMORY_CARD_PROMPT_CHECKING;
    obj             = task->spawnArg2.pointer;
    textColorRgb    = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result     = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[MEMORY_CARD_PROMPT_CHECKING];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->state = task->state + 1;
}

/// Open the current directory entry to read its preview for the save-file list.
///
/// `currentSlot` must index the populated directory, bounded by `entryCount`.
/// Closes the previous file first; success advances to the preview read, and
/// an open error enters the save dialog's access-failed prompt.
static void _mcStateOpenSavePreview(Task* task, McWork* work)
{
    s32 directoryIndex;
    s32 openResult;

    directoryIndex = work->currentSlot;
    MemCardClose();
    openResult       = MemCardOpen(work->channel, work->directory[directoryIndex].name, O_RDONLY);
    work->syncResult = openResult;
    if (openResult == McErrNone) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        task->state = MEMORY_CARD_SAVE_STATE_ACCESS_FAILED;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateReadHeader(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData((u_long*)&work->previews[work->currentSlot], MEMORY_CARD_SAVE_PREVIEW_FILE_OFFSET,
                        sizeof(work->previews[work->currentSlot])) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

static void Mc_StateOpenNext(Task* task, McWork* work)
{
    McWork*       a1;
    Task*         a0;
    UiObject*     obj;
    s32           modeIdx;
    u32           textColorRgb;
    s32           temp_v0;
    McPromptPair* entry;
    McPromptPair* base;

    a1 = work;
    a0 = task;
    if (a1->syncResult == 0) {
        MemCardClose();
        temp_v0         = a1->currentSlot + 1;
        a1->currentSlot = temp_v0;
        if (temp_v0 < a1->entryCount) {
            a0->state = 0x22;
        } else {
            a0->state = a0->state + 1;
        }
    } else {
        a0->state = 0x18;
    }
    obj          = a0->spawnArg2.pointer;
    modeIdx      = a1->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

/// Keep drawing the save prompt until the task countdown allows a card retry.
///
/// Decrements the task's signed frame count once per run and returns to card
/// acceptance at zero or below. The task remains alive throughout this delay.
static void _mcStateDelaySaveRetry(Task* task, McWork* work)
{

    task->killCountdown -= 1;
    if (task->killCountdown <= 0) {
        task->state = MEMORY_CARD_SAVE_STATE_ACCEPT_CARD;
    }
    _mcDrawPrompt(task, work->promptId);
}

/// Keep drawing the save prompt until the task countdown returns to confirmation.
///
/// Decrements the task's signed frame count once per run and returns to the
/// save-game question at zero or below. The task remains alive during the wait.
static void _mcStateDelaySaveConfirmation(Task* task, McWork* work)
{

    task->killCountdown -= 1;
    if (task->killCountdown <= 0) {
        task->state = MEMORY_CARD_SAVE_STATE_CONFIRM_SAVE;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateUiCountdownF(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->cardTimer -= 1;
    if (work->cardTimer <= 0) {
        task->state = 0xF;
    }
    work->promptId = MEMORY_CARD_PROMPT_SAVING;
    obj            = task->spawnArg2.pointer;
    textColorRgb   = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[MEMORY_CARD_PROMPT_SAVING];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

static void Mc_StateEnterPromptE(Task* task, McWork* work)
{
    work->promptId  = MEMORY_CARD_PROMPT_SAVE_FAILED;
    work->cardTimer = 0;
    if (Mc_PromptDialog(task, MEMORY_CARD_PROMPT_SAVE_FAILED, 0) != 0) {
        task->state = 0x13;
    }
}

static void Mc_StateEnterPromptD(Task* task, McWork* work)
{
    work->promptId  = MEMORY_CARD_PROMPT_FORMAT_FAILED;
    work->cardTimer = 0;
    if (Mc_PromptDialog(task, MEMORY_CARD_PROMPT_FORMAT_FAILED, 0) != 0) {
        task->state = 0x13;
    }
}

void Mc_DispatchStateTable(Task* task)
{
    _McSaveStateTable states;
    McWork*           work;
    s32               state;

    states = Mc_PromptStates;
    work   = &Mc_MenuWork;
    state  = task->state;
    if (state < 0) {
        Mc_KillIfCountdown(task, work);
        return;
    }
    states.funcs[state](task, work);
    if (work->cardTimer >= MEMORY_CARD_IO_ABORT_FRAMES) {
        if (work->buffer != 0) {
            memFree(work->buffer);
            work->buffer = 0;
        }
        task->state = 0x18;
    }
    Mc_LastRandomValue = rand();
}

static void Mc_StateInitWorkDefaults(Task* task, McWork* work)
{
    work->promptTimer                            = MEMORY_CARD_PROMPT_LEAD_FRAMES;
    work->promptId                               = MEMORY_CARD_PROMPT_LOAD;
    work->corruptNoticeStyle                     = MEMORY_CARD_CORRUPT_NOTICE_MULTILINE;
    work->cardTimer                              = 0;
    work->buffer                                 = 0;
    work->channel                                = 0;
    gDisplayState.control.flags.pendingPlayerPos = 0;
    task->state                                 += 1;
}

static void Mc_StateSetOpenDefaults(Task* task, McWork* work)
{
    work->slotsRemaining = MEMORY_CARD_BUFFER_SLOT_COUNT;
    work->slotWriteMask  = MEMORY_CARD_SLOT_WRITE_ALL;
    task->state          = 7;
}

static void Mc_StateCountdownPrompt(Task* task, McWork* work)
{
    s32           status;
    u32           textColorRgb;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    if (work->promptTimer > 0) {
        work->promptTimer -= 2;
    }
    if (work->promptTimer == 0) {
        work->promptId = MEMORY_CARD_PROMPT_LOAD;
        status         = Mc_PromptDialogChoice(task, MEMORY_CARD_PROMPT_LOAD, work->promptTimer);
        switch (status) {
            case 0:
                break;
            case 1:
                task->state = 7;
                break;
            case -1:
                task->state = 3;
                break;
        }
    } else {
        obj          = task->spawnArg2.pointer;
        idx          = work->promptId;
        textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
        obj->result  = USER_INTERFACE_RESULT_NONE;
        uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
        base  = Mc_PromptTable;
        entry = &base[idx];
        textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
}

static void Mc_StateCloseReturn(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    MemCardClose();
    idx          = work->promptId;
    obj          = task->spawnArg2.pointer;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->state = 4;
    if (task->spawnArg2.pointer != NULL) {
        ((UiObject*)task->spawnArg2.pointer)->result = USER_INTERFACE_RESULT_CANCEL;
    }
}

static void Mc_StatePromptTimeout(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    work->promptTimer -= 1;
    obj                = task->spawnArg2.pointer;
    idx                = work->promptId;
    textColorRgb       = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result        = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (work->promptTimer < MEMORY_CARD_PROMPT_DISMISS_LIMIT) {
        task->killCountdown = 0;
        task->state         = task->state + 1;
    }
}

static void Mc_KillIfCountdownAlt(Task* task, McWork* unused)
{
    if (task->killCountdown != 0) {
        taskKill(task);
    }
}

static void Mc_StateEnterPromptF(Task* task, McWork* work)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    work->promptId  = MEMORY_CARD_PROMPT_LOAD_FAILED;
    work->cardTimer = 0;
    if (Mc_PromptDialog(task, MEMORY_CARD_PROMPT_LOAD_FAILED, 0) != 0) {
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0       = 0;
        *ptr1       = 0;
        task->state = 3;
    }
}

static void Mc_StateAccept(Task* task, McWork* work)
{
    work->promptId = MEMORY_CARD_PROMPT_CHECKING;
    if (MemCardAccept(work->channel) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateSyncPrompt3(Task* task, McWork* work)
{
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    work->promptId = MEMORY_CARD_PROMPT_NO_CARD;
    if (Mc_PromptDialogFile(task, MEMORY_CARD_PROMPT_NO_CARD, work->promptTimer) != 0) {
        task->state = 3;
        return;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    switch (syncResult) {
        case -1:
            MemCardExist(work->channel);
            return;
        case 1:
            if (work->syncResult != syncResult) {
                child = task->firstChild;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    uiStartTreeClosing(obj, obj->owner);
                    flag->panel.control.word = syncResult;
                }
                task->state = 7;
            }
            return;
        case 0:
            return;
    }
}

static void Mc_StateSyncPromptA(Task* task, McWork* work)
{
    s32       syncResult;
    s32       rslt;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    work->promptId = MEMORY_CARD_PROMPT_NO_DATA;
    if (Mc_PromptDialogFile(task, MEMORY_CARD_PROMPT_NO_DATA, work->promptTimer) != 0) {
        task->state = 3;
        return;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    switch (syncResult) {
        case -1:
            MemCardExist(work->channel);
            return;
        case 1:
            rslt = work->syncResult;
            if (rslt == syncResult) {
                child = task->firstChild;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    uiStartTreeClosing(obj, obj->owner);
                    flag->panel.control.word = rslt;
                }
                task->state = 0xA;
            }
            return;
        case 0:
            return;
    }
}

static void Mc_StateDrawCurrentPrompt(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    work->cardTimer = MEMORY_CARD_IO_BRIEF_FRAMES;
    obj             = task->spawnArg2.pointer;
    idx             = work->promptId;
    textColorRgb    = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result     = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->state = task->state + 1;
}

/// Submit the prepared section read and draw the current prompt until accepted.
///
/// `buffer` must own at least `transferBytes` bytes; `sectorOffset` counts
/// 128-byte sectors, while the SDK takes a byte offset. Acceptance advances to
/// polling; refusal increments `cardTimer`. The buffer remains owned by the
/// dialog until the completion handler frees it.
static void _mcStateReadSection(Task* task, McWork* work)
{

    if (MemCardReadData(work->buffer, work->sectorOffset << MEMORY_CARD_SECTOR_BYTE_SHIFT, work->transferBytes) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateDrawPrompt1(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->cardTimer = MEMORY_CARD_IO_BRIEF_FRAMES;
    work->promptId  = MEMORY_CARD_PROMPT_CHECKING;
    obj             = task->spawnArg2.pointer;
    textColorRgb    = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result     = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[MEMORY_CARD_PROMPT_CHECKING];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    task->state = task->state + 1;
}

static void Mc_StateGetDirentry(Task* task, McWork* work)
{
    u32           textColorRgb;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->cardTimer -= 1;
    if (work->cardTimer == 0) {
        work->entryCount = 0;
        MemCardGetDirentry(
            work->channel, (char*)Mc_SaveFilePattern, work->directory, &work->entryCount, 0,
            MEMORY_CARD_DIRECTORY_CAPACITY);
        if (work->entryCount != 0) {
            work->selectedSlot = 0;
            work->currentSlot  = 0;
            task->state        = task->state + 1;
        } else {
            task->state = 0xB;
        }
    }

    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

static void Mc_StateOpenDirEntry(Task* task, McWork* work)
{
    s32 idx;
    s32 openResult;

    idx = work->currentSlot;
    MemCardClose();
    openResult       = MemCardOpen(work->channel, work->directory[idx].name, 1);
    work->syncResult = openResult;
    if (openResult == 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        task->state = 6;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateReadSlot(Task* task, McWork* work)
{
    u32           textColorRgb;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData((u_long*)&work->previews[work->currentSlot], MEMORY_CARD_SAVE_PREVIEW_FILE_OFFSET,
                        sizeof(work->previews[work->currentSlot])) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    obj          = task->spawnArg2.pointer;
    idx          = work->promptId;
    textColorRgb = uiGetTextColor(obj, USER_INTERFACE_TEXT_COLOR_NORMAL);
    obj->result  = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->upperLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->lowerLine, textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
}

/// Finish a load-list preview read and advance to the next directory entry.
///
/// Called after polling the read. On success, closes the file, increments
/// `currentSlot`, and opens another entry while it is below `entryCount`;
/// otherwise advances to file selection. A read error shows load failure.
/// The directory contains at most fifteen entries; previews borrow work storage.
static void _mcStateAdvanceLoadPreview(Task* task, McWork* work)
{
    s32 nextDirectoryIndex;

    if (work->syncResult == McErrNone) {
        MemCardClose();
        nextDirectoryIndex = work->currentSlot + 1;
        work->currentSlot  = nextDirectoryIndex;
        if (nextDirectoryIndex < work->entryCount) {
            task->state = MEMORY_CARD_LOAD_STATE_OPEN_PREVIEW;
        } else {
            task->state = task->state + 1;
        }
    } else {
        task->state = MEMORY_CARD_LOAD_STATE_FAILED;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateEnterPrompt17(Task* task, McWork* work)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    work->promptId  = MEMORY_CARD_PROMPT_CORRUPTED;
    work->cardTimer = 0;
    if (Mc_PromptDialog(task, MEMORY_CARD_PROMPT_CORRUPTED, 0) != 0) {
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0       = 0;
        *ptr1       = 0;
        task->state = 3;
    }
}

void Mc_DispatchStateTable26(Task* task)
{
    _McFileSelectStateTable states;
    McWork*                 work;

    states = Mc_FileSelectStates;
    work   = &Mc_MenuWork;
    states.funcs[task->state](task, work);
    if (work->cardTimer >= MEMORY_CARD_IO_ABORT_FRAMES) {
        task->state = 6;
    }
}
