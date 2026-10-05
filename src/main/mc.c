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

/// File-list child delay before opening, in nominal 60-Hz UI ticks.
enum { MEMORY_CARD_FILE_LIST_OPENING_DELAY_TICKS = 2 };

/// Destination states in the save dialog's `Mc_PromptStates` table.
enum {
    MEMORY_CARD_SAVE_STATE_DISMISSED         = -1,
    MEMORY_CARD_SAVE_STATE_ACCEPT_CARD       = 2,
    MEMORY_CARD_SAVE_STATE_CONFIRM_CREATE    = 7,
    MEMORY_CARD_SAVE_STATE_CONFIRM_SAVE      = 0xE,
    MEMORY_CARD_SAVE_STATE_PREPARE_SECTION   = 0xF,
    MEMORY_CARD_SAVE_STATE_CLOSE_PROMPT      = 0x13,
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

/// No Yes/No answer has been published on the dialog yet.
enum { MEMORY_CARD_PROMPT_ANSWER_PENDING = 0 };

/// Choice results and nonblocking card-sync statuses used by the save questions.
enum {
    MEMORY_CARD_PROMPT_ANSWER_YES           = 1,
    MEMORY_CARD_PROMPT_ANSWER_NO            = -1,
    MEMORY_CARD_SYNC_IDLE                   = -1,
    MEMORY_CARD_SYNC_PENDING                = 0,
    MEMORY_CARD_SYNC_COMPLETE               = 1,
    MEMORY_CARD_SYNC_POLL                   = 1,
    MEMORY_CARD_SAVE_RETRY_DELAY_FRAMES     = 12,
    MEMORY_CARD_PROMPT_APPROACH_STEP_FRAMES = 2,
};

/// Additional destinations of the save questions and load failure notices.
enum {
    MEMORY_CARD_SAVE_STATE_BEGIN_OPEN_FILE    = 5,
    MEMORY_CARD_SAVE_STATE_BEGIN_CREATE       = 8,
    MEMORY_CARD_SAVE_STATE_BEGIN_FORMAT       = 0x16,
    MEMORY_CARD_SAVE_STATE_DELAY_RETRY        = 0x27,
    MEMORY_CARD_SAVE_STATE_DELAY_SECTION      = 0x28,
    MEMORY_CARD_SAVE_STATE_DELAY_CONFIRMATION = 0x29,
    MEMORY_CARD_LOAD_STATE_CLOSE_FILE         = 3,
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

static void _mcBuildSaveFileName(u8* filenameCursor, s32 directoryIndex);

static void _mcSeedNewGameRecords(void);

static inline void _mcWriteBlockChecksum(u8* recordBytes, s32 recordByteCount);

static s32 _mcUpdateOkPrompt(Task* dialogTask, s32 promptId, s32 unusedPromptTimer);

static s32 _mcUpdateYesNoPrompt(Task* dialogTask, s32 promptId, s32 unusedPromptTimer);

static s32 _mcUpdateYesNoPromptInitialNo(Task* dialogTask, s32 promptId, s32 unusedPromptTimer);

static s32 _mcUpdateCancelPrompt(Task* dialogTask, s32 promptId, s32 unusedPromptTimer);

static inline u16* _mcAppendAsciiTitleText(const u8* asciiText, u16* titleCursor);

static inline u16* _mcAppendAsciiTitleLiteral(const char* asciiText, u16* titleCursor);

static inline void _mcWriteSaveHeaderChecksum(void);

static inline u16* _mcAppendEncodedTitleLabel(const u8* encodedLabel, u16* titleCursor);

static inline void _mcWriteCardHeaderChecksum(void);

static void _mcBuildCardFileTitle(const McWork* work);

static void _mcStateScanCardBlocks(Task* task, McWork* work);

static void _mcStateReadSaveDirectory(Task* task, McWork* work);

static inline void _mcDrawPrompt(Task* task, s32 promptId);

static inline void _mcCloseChildUi(Task* task, s32 parentInputControl);

static inline void _mcCopyFileName(s32 restoreSavedName);

static inline void _mcCloseFileList(UiObject* listObject, UiPanel* dialogPanel);

static void _mcStateSelectSaveFile(Task* dialogTask, McWork* dialogWork);

static inline s32 _mcGetSectionWriteMask(void);

static void _mcStateResolveSaveCardProbe(Task* task, McWork* work);

static void _mcStateOpenSaveFileForWrite(Task* task, McWork* work);

static void _mcStateCreateSaveFile(Task* task, McWork* work);

static void _mcStateResolveFileHeaderWrite(Task* task, McWork* work);

static void _mcStateConfirmSaveOverwrite(Task* task, McWork* work);

static inline void _mcBackupSaveSections(void);

static inline void _mcWriteSectionChecksumSummary(void);

static void _mcStatePrepareSectionWrite(Task* task, McWork* work);

static void _mcStateFinishSectionWrite(Task* task, McWork* work);

static void _mcStateFormatCard(Task* task, McWork* work);

static void _mcStateSelectLoadFile(Task* dialogTask, McWork* dialogWork);

static void _mcStateResolveLoadCardProbe(Task* task, McWork* work);

static void _mcStateOpenSaveFileForRead(Task* task, McWork* work);

static inline s32 _mcVerifySaveSectionChecksums(void);

static inline void _mcWriteSaveSectionChecksums(void);

static inline s32 _mcVerifySectionChecksumSummary(void);

static void _mcStatePrepareSectionRead(Task* dialogTask, McWork* dialogWork);

static inline void _mcWriteReadCardHeaderChecksum(McWork* work);

static void _mcStateFinishSectionRead(Task* task, McWork* work);

static inline s32 _mcVerifySavePreviewChecksum(const McSavePreview* preview);

static void _mcDrawLoadFileRow(UiList* list, UiObject* object);

static u16* _mcEncodeAsciiTitleText(const u8* asciiText, u16* titleCursor);

static void _mcResetFileNameSuffixes(void);

static void _mcCopyCardFileName(s32 restoreSavedName);

static void _mcWriteLiveSaveHeaderChecksum(void);

static s32 _mcVerifySaveHeaderChecksum(const McSaveData* save);

static void _mcWriteRecordChecksum(u8* recordBytes, s32 recordByteCount);

static void _mcResetStageFlagRecords(void);

static s32 _mcVerifyRecordChecksum(const u8* recordBytes, s32 recordByteCount);

static void _mcNoOp(void);

static s32 _mcQuerySectionWriteMask(void);

static void _mcWriteLiveSaveSectionChecksums(void);

static void _mcWriteLiveSectionChecksumSummary(void);

static s32 _mcCheckSectionChecksumSummary(void);

static s32 _mcCheckSaveSectionChecksums(void);

static void _mcBackupLiveSaveSections(void);

static void _mcDrawDialogPrompt(Task* dialogTask, s32 promptId);

static void _mcCloseChildAndReactivateDialog(Task* dialogTask);

static void _mcWriteSelectedCardHeaderChecksum(s32 useReadBuffer, McWork* work);

static s32 _mcMatchesCardHeaderChecksum(const McSaveData* save, const McWork* work);

static void _mcStateInitSaveWork(Task* task, McWork* work);

static void _mcStatePrepareSaveSections(Task* task, McWork* work);

static void _mcStateAcceptSaveCard(Task* task, McWork* work);

static void _mcStatePollCardAdvance(Task* task, McWork* work);

static void _mcStateBeginOpenSaveFile(Task* task, McWork* work);

static void _mcStateConfirmFileCreation(Task* task, McWork* work);

static void _mcStateBeginCreateFile(Task* task, McWork* work);

static void _mcStateBeginFileHeaderWrite(Task* task, McWork* work);

static void _mcStateWriteFileHeader(Task* task, McWork* work);

static void _mcStateConfirmSave(Task* task, McWork* work);

static void _mcStateWriteSection(Task* task, McWork* work);

static void _mcStateCloseSaveFile(Task* task, McWork* work);

static void _mcStateKillSaveDialogIfRequested(Task* task, McWork* unusedWork);

static void _mcStateWaitSaveCardInsertion(Task* task, McWork* work);

static void _mcStateConfirmCardFormat(Task* task, McWork* work);

static void _mcStateBeginCardFormat(Task* task, McWork* work);

static void _mcStateWaitFullCardRemoval(Task* task, McWork* work);

static void _mcStateAcknowledgeSaveAccessFailure(Task* task, McWork* work);

static void _mcStateDismissSavePrompt(Task* task, McWork* work);

static void _mcStateRestartSaveDirectory(Task* task, McWork* work);

static void _mcStateWaitSaveClose(Task* task, McWork* work);

static void _mcStateBeginSaveDirectory(Task* task, McWork* work);

static void _mcStateOpenSavePreview(Task* task, McWork* work);

static void _mcStateReadSavePreview(Task* task, McWork* work);

static void _mcStateAdvanceSavePreview(Task* task, McWork* work);

static void _mcStateDelaySaveRetry(Task* task, McWork* work);

static void _mcStateDelaySaveConfirmation(Task* task, McWork* work);

static void _mcStateDelaySectionWrite(Task* task, McWork* work);

static void _mcStateAcknowledgeSaveFailure(Task* task, McWork* work);

static void _mcStateAcknowledgeFormatFailure(Task* task, McWork* work);

static void _mcStateInitLoadWork(Task* task, McWork* work);

static void _mcStateInitLoadSections(Task* task, McWork* work);

static void _mcStateConfirmLoadAfterDelay(Task* task, McWork* work);

static void _mcStateCloseLoadFile(Task* task, McWork* work);

static void _mcStateDismissLoadPrompt(Task* task, McWork* work);

static void _mcStateKillLoadDialogIfRequested(Task* task, McWork* unusedWork);

static void _mcStateAcknowledgeLoadFailure(Task* task, McWork* work);

static void _mcStateAcceptLoadCard(Task* task, McWork* work);

static void _mcStateWaitLoadCardInsertion(Task* task, McWork* work);

static void _mcStateWaitNoDataCardRemoval(Task* dialogTask, McWork* dialogWork);

static void _mcStateBeginSectionLoad(Task* task, McWork* work);

static void _mcStateReadSection(Task* task, McWork* work);

static void _mcStateBeginLoadDirectory(Task* task, McWork* work);

static void _mcStateReadLoadDirectory(Task* task, McWork* work);

static void _mcStateOpenLoadPreview(Task* task, McWork* work);

static void _mcStateReadLoadPreview(Task* task, McWork* work);

static void _mcStateAdvanceLoadPreview(Task* task, McWork* work);

static void _mcStateAcknowledgeCorruptLoad(Task* task, McWork* work);

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

static UiListRowCallback Mc_SaveSlotCallbacks[] = { _mcDrawLoadFileRow };
UiList                   Mc_SaveSlotList        = { Mc_SaveSlotCallbacks, 0x0F, 0x0F, 0, 0x2E };
static UiListRowCallback Mc_LoadSlotCallbacks[] = { mcMenuDrawSaveFileRow };
UiList                   Mc_LoadSlotList        = { Mc_LoadSlotCallbacks, 0x0F, 0x0F, 0, 0x2E };

static u8* Mc_ModeLabels[] = { (u8*)McText_Replay, (u8*)McText_Bounty, (u8*)McText_Scavenger, (u8*)McText_Nightmare };

UiObjectDesc Mc_TaskDescriptors[] = {
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -25, 0x120, 0x32 }, 0x14, 0, TASK_BODY_NONE, 0xC0, Mc_DispatchStateTable26, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -25, 0x120, 0x32 }, 0x14, 0, TASK_BODY_NONE, 0xC0, Mc_DispatchStateTable, 0 },
};
static UiObjectDesc Mc_SaveListDesc[] = {
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { -136, 10, 0x120, 0x3C }, 0x0C, 0, TASK_BODY_NONE, 0xC0, mcMenuUpdateLoadFileList, 0 },
};
static UiObjectDesc Mc_LoadListDescriptors[] = {
    { 0x80000 | USER_INTERFACE_PANEL_TITLE_STYLE, { -136, 10, 0x120, 0x3C }, 0x0C, 0, TASK_BODY_NONE, 0xC0, mcMenuUpdateSaveFileList, 0 },
    { USER_INTERFACE_PANEL_TITLE_STYLE, { -60, 30, 0xC8, 0x3C }, 0x1C, 0, TASK_BODY_NONE, 0xC0, mcMenuUpdateFileInformation, 0 },
};

/// Build a new card filename from the product prefix, directory index and random suffix.
///
/// The destination needs 21 writable bytes: twelve product bytes, one alphabet
/// character selected by `directoryIndex`, seven random characters and NUL.
/// The index must be in 0..63; new-save callers use the first free directory row.
/// Consumes seven `rand()` values and leaves any later backing bytes intact.
static void _mcBuildSaveFileName(u8* filenameCursor, s32 directoryIndex)
{
    enum {
        MEMORY_CARD_FILENAME_PREFIX_BYTES = 12,
        MEMORY_CARD_FILENAME_RANDOM_MASK  = ARRAY_SIZE(Mc_FileNameAlphabet) - 1,
    };
    s32 prefixByteIndex;

    prefixByteIndex = 0;
    do {
        *filenameCursor = Mc_SaveFilePattern[prefixByteIndex];
        prefixByteIndex++;
        filenameCursor++;
    } while (prefixByteIndex < MEMORY_CARD_FILENAME_PREFIX_BYTES);

    // Skip the directory pattern's wildcard and generate the eight-byte suffix.
    *filenameCursor   = Mc_FileNameAlphabet[directoryIndex];
    *++filenameCursor = Mc_FileNameAlphabet[rand() & MEMORY_CARD_FILENAME_RANDOM_MASK];
    *++filenameCursor = Mc_FileNameAlphabet[rand() & MEMORY_CARD_FILENAME_RANDOM_MASK];
    *++filenameCursor = Mc_FileNameAlphabet[rand() & MEMORY_CARD_FILENAME_RANDOM_MASK];
    *++filenameCursor = Mc_FileNameAlphabet[rand() & MEMORY_CARD_FILENAME_RANDOM_MASK];
    *++filenameCursor = Mc_FileNameAlphabet[rand() & MEMORY_CARD_FILENAME_RANDOM_MASK];
    *++filenameCursor = Mc_FileNameAlphabet[rand() & MEMORY_CARD_FILENAME_RANDOM_MASK];
    *++filenameCursor = Mc_FileNameAlphabet[rand() & MEMORY_CARD_FILENAME_RANDOM_MASK];
    filenameCursor[1] = 0;
}

/// Clear the five live stage-flag banks and mark their backups as changed.
///
/// Covers each complete bank, including its checksum header, visited-area
/// bits, object states and any area records. The adjacent backups receive
/// 0xFF so the next save comparison selects every stage bank. No checksums are
/// recomputed; the separate packed-nibble bank is outside this reset.
static inline void _mcResetStageFlagCopies(void)
{
    enum {
        MEMORY_CARD_STAGE_FLAG_COPY_LIVE   = 0,
        MEMORY_CARD_STAGE_FLAG_COPY_BACKUP = 1,
    };

    GameFlagAcropolisBank*     acropolisBanks;
    GameFlagDryfieldBank*      dryfieldBanks;
    GameFlagDryfieldNightBank* dryfieldNightBanks;
    GameFlagMineShelterBank*   mineShelterBanks;
    GameFlagNeoArkBank*        neoArkBanks;

    acropolisBanks = &GameFlag_AcropolisBanks[MEMORY_CARD_STAGE_FLAG_COPY_LIVE];
    memFillBytes(acropolisBanks, 0, sizeof(*acropolisBanks));
    dryfieldBanks = &GameFlag_DryfieldBanks[MEMORY_CARD_STAGE_FLAG_COPY_LIVE];
    memFillBytes(dryfieldBanks, 0, sizeof(*dryfieldBanks));
    dryfieldNightBanks = &GameFlag_DryfieldFullBanks[MEMORY_CARD_STAGE_FLAG_COPY_LIVE];
    memFillBytes(dryfieldNightBanks, 0, sizeof(*dryfieldNightBanks));
    mineShelterBanks = &GameFlag_ShelterBanks[MEMORY_CARD_STAGE_FLAG_COPY_LIVE];
    memFillBytes(mineShelterBanks, 0, sizeof(*mineShelterBanks));
    neoArkBanks = &GameFlag_NeoArkBanks[MEMORY_CARD_STAGE_FLAG_COPY_LIVE];
    memFillBytes(neoArkBanks, 0, sizeof(*neoArkBanks));
    memFillBytes(&acropolisBanks[MEMORY_CARD_STAGE_FLAG_COPY_BACKUP], MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*acropolisBanks));
    memFillBytes(&dryfieldBanks[MEMORY_CARD_STAGE_FLAG_COPY_BACKUP], MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*dryfieldBanks));
    memFillBytes(&dryfieldNightBanks[MEMORY_CARD_STAGE_FLAG_COPY_BACKUP], MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*dryfieldNightBanks));
    memFillBytes(&mineShelterBanks[MEMORY_CARD_STAGE_FLAG_COPY_BACKUP], MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*mineShelterBanks));
    memFillBytes(&neoArkBanks[MEMORY_CARD_STAGE_FLAG_COPY_BACKUP], MEMORY_CARD_BACKUP_FILL_BYTE, sizeof(*neoArkBanks));
}

/// Restore the six live save options and immediately apply stereo and music gain.
///
/// Selects vibration on, layout A, maximum music, remembered cursor and walking.
/// Leaves the backup and record checksums unchanged; audio requests are applied
/// after the saved option bytes have been written.
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

/// Position an OK or Cancel child below its dialog and suspend dialog input.
///
/// Call once after spawning the active choice child under the live dialog.
/// Layout values are unsigned pixels relative to the screen center; additions
/// narrow to 16-bit bounds. Aligns the child's right edge five pixels beyond
/// the dialog's content right edge and places it eight pixels below the bottom.
/// Clears the dialog answer to pending; the child's task owns its lifetime.
static inline void _mcInitSingleActionPromptChild(UiObject* dialogObject, UiObject* choiceObject)
{
    enum {
        MEMORY_CARD_CHOICE_RIGHT_OFFSET_PIXELS = 5,
        MEMORY_CARD_CHOICE_BELOW_DIALOG_PIXELS = 8
    };

    choiceObject->panel.bounds.unsignedRect.x = (dialogObject->panel.contentOriginX.unsignedValue + dialogObject->panel.contentRight.unsignedValue + MEMORY_CARD_CHOICE_RIGHT_OFFSET_PIXELS) - choiceObject->panel.bounds.unsignedRect.w;
    choiceObject->panel.bounds.unsignedRect.y = dialogObject->panel.contentOriginY.unsignedValue + dialogObject->panel.contentBottom.unsignedValue + MEMORY_CARD_CHOICE_BELOW_DIALOG_PIXELS;
    dialogObject->resultValue                 = MEMORY_CARD_PROMPT_ANSWER_PENDING;
    dialogObject->panel.control.word          = USER_INTERFACE_PANEL_INACTIVE;
}

/// Latch a confirmed prompt answer, detach its closing child and restore input.
///
/// Requires a live dialog and its live, confirmed choice child. Copies only
/// `resultValue`: OK or Cancel publishes 1, Yes/No publishes 1 or -1.
/// Closing detaches the child tree; its UI task releases the objects later.
/// The caller must leave the answered state before spawning another choice.
static inline void _mcAcceptPromptAnswer(UiObject* dialogObject, UiObject* choiceObject)
{
    dialogObject->resultValue = choiceObject->resultValue;
    uiStartTreeClosing(choiceObject, choiceObject->owner);
    dialogObject->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
}

/// Draw an acknowledgement prompt and create or poll its OK child.
///
/// The task borrows a live dialog object and its first child, if any, must be
/// this prompt's choice object. `promptId` indexes `Mc_PromptTable`. Returns
/// 0 while waiting or allocation fails, 1 on acknowledgement. Confirmation
/// closes the child and restores dialog input; leave the state after an answer.
/// The callers' lead-in timer is retained but unread: opening is not delayed.
static s32 _mcUpdateOkPrompt(Task* dialogTask, s32 promptId, s32 unusedPromptTimer)
{
    enum { MEMORY_CARD_CHOICE_OPEN_DELAY_TICKS = 2 };
    UiObject* dialogObject;
    Task*     choiceTask;
    UiObject* choiceObject;

    dialogObject = dialogTask->spawnArg2.pointer;
    _mcDrawPrompt(dialogTask, promptId);

    choiceTask = dialogTask->firstChild;
    if (choiceTask == NULL) {
        UiObject* spawnedChoice;

        spawnedChoice = uiSpawnObject(Mc_PromptDesc, MEMORY_CARD_MENU_PROMPT_OK, USER_INTERFACE_PANEL_ACTIVE, MEMORY_CARD_CHOICE_OPEN_DELAY_TICKS, dialogObject);
        if (spawnedChoice != NULL) {
            _mcInitSingleActionPromptChild(dialogObject, spawnedChoice);
        }
        return MEMORY_CARD_PROMPT_ANSWER_PENDING;
    }
    choiceObject = choiceTask->spawnArg2.pointer;
    if (choiceObject->result == USER_INTERFACE_RESULT_CONFIRM) {
        _mcAcceptPromptAnswer(dialogObject, choiceObject);
    }
    return dialogObject->resultValue;
}

/// Position a Yes/No child below its dialog and suspend dialog input.
///
/// Call once after spawning the active choice child under the live dialog.
/// Uses unsigned screen-centered pixel layout values, narrowing to 16-bit bounds.
/// The child's right edge is five pixels beyond the content right edge, and its
/// top is sixteen pixels below the bottom. Clears the dialog answer to pending;
/// initial Yes/No selection and the child's lifetime belong to its UI task.
static inline void _mcInitYesNoPromptChild(UiObject* dialogObject, UiObject* choiceObject)
{
    enum {
        MEMORY_CARD_CHOICE_RIGHT_OFFSET_PIXELS = 5,
        MEMORY_CARD_CHOICE_BELOW_DIALOG_PIXELS = 16
    };

    choiceObject->panel.bounds.unsignedRect.x = (dialogObject->panel.contentOriginX.unsignedValue + dialogObject->panel.contentRight.unsignedValue + MEMORY_CARD_CHOICE_RIGHT_OFFSET_PIXELS) - choiceObject->panel.bounds.unsignedRect.w;
    choiceObject->panel.bounds.unsignedRect.y = dialogObject->panel.contentOriginY.unsignedValue + dialogObject->panel.contentBottom.unsignedValue + MEMORY_CARD_CHOICE_BELOW_DIALOG_PIXELS;
    dialogObject->resultValue                 = MEMORY_CARD_PROMPT_ANSWER_PENDING;
    dialogObject->panel.control.word          = USER_INTERFACE_PANEL_INACTIVE;
}

/// Draw a memory-card question and create or poll its Yes/No child.
///
/// `dialogTask` must own the live `UiObject` in its second spawn argument;
/// any first child must own this question's choice object. `promptId` indexes
/// `Mc_PromptTable`. Yes is initially selected. Returns 0 while waiting or
/// allocation fails, 1 for Yes or -1 for No. Confirmation latches the answer,
/// detaches and starts closing the child, and restores parent input. The caller
/// must leave this state when answered, since another call can spawn a new child.
/// The UI task owns the child and releases it after animated closing.
/// `unusedPromptTimer` retains the callers' lead-in timer argument; this
/// routine does not read it or delay opening the choices.
static s32 _mcUpdateYesNoPrompt(Task* dialogTask, s32 promptId, s32 unusedPromptTimer)
{
    enum { MEMORY_CARD_CHOICE_OPEN_DELAY_TICKS = 2 };
    UiObject* dialogObject;
    Task*     choiceTask;
    UiObject* choiceObject;

    dialogObject = dialogTask->spawnArg2.pointer;
    _mcDrawPrompt(dialogTask, promptId);

    choiceTask = dialogTask->firstChild;
    if (choiceTask == NULL) {
        UiObject* spawnedChoice;

        spawnedChoice = uiSpawnObject(Mc_PromptDesc, MEMORY_CARD_MENU_PROMPT_YES_NO, USER_INTERFACE_PANEL_ACTIVE, MEMORY_CARD_CHOICE_OPEN_DELAY_TICKS, dialogObject);
        if (spawnedChoice != NULL) {
            _mcInitYesNoPromptChild(dialogObject, spawnedChoice);
        }
        return MEMORY_CARD_PROMPT_ANSWER_PENDING;
    }
    choiceObject = choiceTask->spawnArg2.pointer;
    if (choiceObject->result == USER_INTERFACE_RESULT_CONFIRM) {
        // Latch the answer before closing releases the child and restores input.
        dialogObject->resultValue = choiceObject->resultValue;
        uiStartTreeClosing(choiceObject, choiceObject->owner);
        dialogObject->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
    }
    return dialogObject->resultValue;
}

/// Draw a memory-card question and create or poll its initially selected No row.
///
/// Uses the same object, prompt-row and child-lifetime contract as
/// `_mcUpdateYesNoPrompt`. Returns 0 while pending, 1 for Yes or -1 for No.
/// The callers' lead-in timer is retained but unread; leave the state on answer.
static s32 _mcUpdateYesNoPromptInitialNo(Task* dialogTask, s32 promptId, s32 unusedPromptTimer)
{
    enum { MEMORY_CARD_CHOICE_OPEN_DELAY_TICKS = 2 };
    UiObject* dialogObject;
    Task*     choiceTask;
    UiObject* choiceObject;

    dialogObject = dialogTask->spawnArg2.pointer;
    _mcDrawPrompt(dialogTask, promptId);

    choiceTask = dialogTask->firstChild;
    if (choiceTask == NULL) {
        UiObject* spawnedChoice;

        spawnedChoice = uiSpawnObject(Mc_PromptDesc, MEMORY_CARD_MENU_PROMPT_YES_NO_INITIAL_NO, USER_INTERFACE_PANEL_ACTIVE, MEMORY_CARD_CHOICE_OPEN_DELAY_TICKS, dialogObject);
        if (spawnedChoice != NULL) {
            _mcInitYesNoPromptChild(dialogObject, spawnedChoice);
        }
        return MEMORY_CARD_PROMPT_ANSWER_PENDING;
    }
    choiceObject = choiceTask->spawnArg2.pointer;
    if (choiceObject->result == USER_INTERFACE_RESULT_CONFIRM) {
        _mcAcceptPromptAnswer(dialogObject, choiceObject);
    }
    return dialogObject->resultValue;
}

/// Draw a card-recovery prompt and create or poll its single Cancel action.
///
/// Borrows the dialog and choice child as `_mcUpdateOkPrompt` does. Returns
/// 0 while pending or allocation fails, 1 when Cancel is confirmed; the parent
/// handles card probes independently. The child has an 18-pixel outer height.
/// The callers' lead-in timer is retained but unread; leave the state on answer.
static s32 _mcUpdateCancelPrompt(Task* dialogTask, s32 promptId, s32 unusedPromptTimer)
{
    enum { MEMORY_CARD_CHOICE_OPEN_DELAY_TICKS    = 2,
           MEMORY_CARD_CANCEL_PANEL_HEIGHT_PIXELS = 18 };
    UiObject* dialogObject;
    Task*     choiceTask;
    UiObject* choiceObject;

    dialogObject = dialogTask->spawnArg2.pointer;
    _mcDrawPrompt(dialogTask, promptId);

    choiceTask = dialogTask->firstChild;
    if (choiceTask == NULL) {
        UiObject* spawnedChoice;

        spawnedChoice = uiSpawnObject(Mc_PromptDesc, MEMORY_CARD_MENU_PROMPT_CANCEL, USER_INTERFACE_PANEL_ACTIVE, MEMORY_CARD_CHOICE_OPEN_DELAY_TICKS, dialogObject);
        if (spawnedChoice != NULL) {
            spawnedChoice->panel.bounds.unsignedRect.h = MEMORY_CARD_CANCEL_PANEL_HEIGHT_PIXELS;
            _mcInitSingleActionPromptChild(dialogObject, spawnedChoice);
        }
        return MEMORY_CARD_PROMPT_ANSWER_PENDING;
    }
    choiceObject = choiceTask->spawnArg2.pointer;
    if (choiceObject->result == USER_INTERFACE_RESULT_CONFIRM) {
        _mcAcceptPromptAnswer(dialogObject, choiceObject);
    }
    return dialogObject->resultValue;
}

/// Append ASCII byte text as full-width Shift-JIS glyphs to a card title.
///
/// Accepts a NUL-terminated string of letters and characters from space through
/// '@'. The source is borrowed and read-only. The halfword-aligned destination
/// must not overlap it and needs one halfword per character plus a zero halfword.
/// Returns the destination's terminator for the next append; no capacity is checked.
static inline u16* _mcAppendAsciiTitleText(const u8* asciiText, u16* titleCursor)
{
    s32 asciiCode;
    u8  asciiByte;

    // Decode through signed bytes while accepting the formatter's byte strings.
    asciiByte = *asciiText;
    if (*(const s8*)asciiText != 0) {
        do {
            asciiCode = (s8)asciiByte;
            if (asciiCode >= 'a') {
                *titleCursor = Mc_GlyphsLower[asciiCode - 'a'];
            } else if (asciiCode >= 'A') {
                *titleCursor = Mc_GlyphsUpper[asciiCode - 'A'];
            } else if (asciiCode >= ' ') {
                *titleCursor = Mc_GlyphsSymbol[asciiCode - ' '];
            }
            asciiText++;
            asciiByte = *asciiText;
            titleCursor++;
        } while (*(const s8*)asciiText != 0);
    }
    *titleCursor = 0;
    return titleCursor;
}

/// Append an ASCII C string as full-width Shift-JIS glyphs to a card title.
///
/// Accepts NUL-terminated letters and characters from space through '@'.
/// The source is borrowed and read-only; it must not overlap the halfword-aligned
/// destination, which needs one halfword per character plus a zero halfword.
/// Returns the destination's terminator for the next append; no capacity is checked.
static inline u16* _mcAppendAsciiTitleLiteral(const char* asciiText, u16* titleCursor)
{
    s32 asciiCode;

    // Decode the C string through the signed-byte glyph input.
    if (*(const s8*)asciiText != 0) {
        do {
            asciiCode = *(const s8*)asciiText;
            if (asciiCode >= 'a') {
                *titleCursor = Mc_GlyphsLower[asciiCode - 'a'];
            } else if (asciiCode >= 'A') {
                *titleCursor = Mc_GlyphsUpper[asciiCode - 'A'];
            } else if (asciiCode >= ' ') {
                *titleCursor = Mc_GlyphsSymbol[asciiCode - ' '];
            }
            asciiText++;
            titleCursor++;
        } while (*(const s8*)asciiText != 0);
    }
    *titleCursor = 0;
    return titleCursor;
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

/// Choose a place-local save number and rebuild the card file title and checksum pairs.
///
/// Borrows at most fifteen cached previews. The live save's place index must
/// select one of the seventeen title labels (0..16). Appends full-width Shift-JIS
/// `PE2 H:MM Place(number)` beginning at header byte 4, increments the save count
/// up to 99 (255 starts at zero), then updates the preview and file-header sums.
/// Save numbers are 1..99: after 99, the first unused number at that place is chosen.
/// The section-checksum summary is cleared for the subsequent record preparation.
/// Appends are unchecked; the longest current title plus terminator is 66 bytes,
/// within the 512-byte header, and can extend past the 64-byte title region.
static void _mcBuildCardFileTitle(const McWork* work)
{
    enum {
        MEMORY_CARD_TITLE_BYTE_OFFSET = 4,
        MEMORY_CARD_TITLE_BYTES       = 64,
        MEMORY_CARD_SAVE_NUMBER_FIRST = 1,
        MEMORY_CARD_SAVE_NUMBER_LIMIT = 100,
        MEMORY_CARD_SAVE_COUNT_MAX    = 99,
        MEMORY_CARD_SAVE_COUNT_NEW    = 0xFF,
    };
    u8                   formatScratch[0x20];
    u16*                 titleCursor;
    s32                  saveNumber;
    s32                  previewIndex;
    s32                  candidateNumber;
    s32                  numberAvailable;
    const McSavePreview* preview;

    // Continue this place's numbering, wrapping to its first unused number.
    titleCursor = (u16*)(Mc_DefaultChecksumSrc + MEMORY_CARD_TITLE_BYTE_OFFSET);
    saveNumber  = MEMORY_CARD_SAVE_NUMBER_FIRST;
    if (work->entryCount > 0) {
        for (previewIndex = 0; previewIndex < work->entryCount; previewIndex++) {
            preview = &work->previews[previewIndex];
            if ((s8)preview->savePoint == (s8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint) {
                if (preview->saveNumber >= saveNumber) {
                    saveNumber = preview->saveNumber + 1;
                }
            }
        }
        if (saveNumber >= MEMORY_CARD_SAVE_NUMBER_LIMIT) {
            for (candidateNumber = MEMORY_CARD_SAVE_NUMBER_FIRST; candidateNumber < MEMORY_CARD_SAVE_NUMBER_LIMIT; candidateNumber++) {
                numberAvailable = 1;
                for (previewIndex = 0; previewIndex < work->entryCount; previewIndex++) {
                    preview = &work->previews[previewIndex];
                    if ((s8)preview->savePoint == (s8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint && preview->saveNumber == candidateNumber) {
                        numberAvailable = 0;
                        break;
                    }
                }
                if (numberAvailable == 1) {
                    saveNumber = candidateNumber;
                    break;
                }
            }
        }
    }
    // ASCII fragments become halfword glyphs; the place label is already encoded.
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveNumber                                = saveNumber;
    titleCursor                                                                        = _mcAppendAsciiTitleLiteral("PE2 ", titleCursor);
    titleCursor                                                                        = _mcAppendAsciiTitleText(textFormatPlayTime(formatScratch, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime), titleCursor);
    titleCursor                                                                        = _mcAppendAsciiTitleLiteral(" ", titleCursor);
    Mc_DefaultChecksumSrc[MEMORY_CARD_TITLE_BYTE_OFFSET + MEMORY_CARD_TITLE_BYTES - 1] = 0;
    Mc_DefaultChecksumSrc[MEMORY_CARD_TITLE_BYTE_OFFSET + MEMORY_CARD_TITLE_BYTES - 2] = 0;
    titleCursor                                                                        = _mcAppendEncodedTitleLabel(Mc_LocationTitleLabels[(s8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint], titleCursor);
    titleCursor                                                                        = _mcAppendAsciiTitleLiteral("(", titleCursor);
    titleCursor                                                                        = _mcAppendAsciiTitleText(textItoaSigned(formatScratch, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveNumber), titleCursor);
    titleCursor                                                                        = _mcAppendAsciiTitleLiteral(McText_CloseParen, titleCursor);
    *titleCursor                                                                       = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount == MEMORY_CARD_SAVE_COUNT_NEW) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount = 0;
    } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount < MEMORY_CARD_SAVE_COUNT_MAX) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount++;
    }
    _mcWriteSaveHeaderChecksum();
    _mcWriteCardHeaderChecksum();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksum           = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksumComplement = MEMORY_CARD_CHECKSUM_MASK;
}

static const char McText_CloseParen[] = ")";

static const _McSaveStateTable Mc_PromptStates = { {
    _mcStateInitSaveWork,
    _mcStatePrepareSaveSections,
    _mcStateAcceptSaveCard,
    _mcStatePollCardAdvance,
    _mcStateResolveSaveCardProbe,
    _mcStateBeginOpenSaveFile,
    _mcStateOpenSaveFileForWrite,
    _mcStateConfirmFileCreation,
    _mcStateBeginCreateFile,
    _mcStateCreateSaveFile,
    _mcStateBeginFileHeaderWrite,
    _mcStateWriteFileHeader,
    _mcStatePollCardAdvance,
    _mcStateResolveFileHeaderWrite,
    _mcStateConfirmSave,
    _mcStatePrepareSectionWrite,
    _mcStateWriteSection,
    _mcStatePollCardAdvance,
    _mcStateFinishSectionWrite,
    _mcStateCloseSaveFile,
    _mcStateWaitSaveCardInsertion,
    _mcStateConfirmCardFormat,
    _mcStateBeginCardFormat,
    _mcStateFormatCard,
    _mcStateAcknowledgeSaveAccessFailure,
    _mcStateWaitFullCardRemoval,
    _mcStateConfirmSaveOverwrite,
    _mcStateDismissSavePrompt,
    _mcStateRestartSaveDirectory,
    _mcStateWaitSaveClose,
    _mcStateKillSaveDialogIfRequested,
    _mcStateBeginSaveDirectory,
    _mcStateScanCardBlocks,
    _mcStateReadSaveDirectory,
    _mcStateOpenSavePreview,
    _mcStateReadSavePreview,
    _mcStatePollCardAdvance,
    _mcStateAdvanceSavePreview,
    _mcStateSelectSaveFile,
    _mcStateDelaySaveRetry,
    _mcStateDelaySectionWrite,
    _mcStateDelaySaveConfirmation,
    _mcStateAcknowledgeSaveFailure,
    _mcStateAcknowledgeFormatFailure,
} };

/// Scan every card file and mark its occupied blocks after the directory delay.
///
/// Entry requires a positive frame timer. The SDK fills at most fifteen entries;
/// its result is ignored. Each head is a sector number beyond the directory block,
/// and its contiguous extent must lie in the fifteen usable blocks. Records the
/// rounded-up block total for the following product-only scan, then advances.
/// The sixteenth ownership byte is retained. Draws the current prompt each run.
static void _mcStateScanCardBlocks(Task* task, McWork* work)
{
    s32 directoryIndex;
    s32 fileBlockIndex;
    s32 fileBytes;
    s32 fileBlockCount;
    s32 firstDataBlock;

    work->cardTimer -= 1;
    if (work->cardTimer == 0) {
        work->entryCount = 0;
        // Mark the fifteen usable blocks free, then claim each file's blocks.
        for (directoryIndex = 0; directoryIndex < MEMORY_CARD_BLOCK_COUNT; directoryIndex++) {
            work->blockOwners[directoryIndex] = MEMORY_CARD_BLOCK_FREE;
        }
        MemCardGetDirentry(
            work->channel, "*", work->directory, &work->entryCount, 0,
            ARRAY_SIZE(work->directory));

        work->foreignBlockCount = 0;
        if (work->entryCount != 0) {
            for (directoryIndex = 0; directoryIndex < work->entryCount; directoryIndex++) {
                fileBytes      = work->directory[directoryIndex].size;
                firstDataBlock = work->directory[directoryIndex].head;
                fileBlockCount = fileBytes / MEMORY_CARD_BLOCK_BYTES + ((fileBytes % MEMORY_CARD_BLOCK_BYTES) != 0);
                // Sector-to-block conversion excludes block 0, the card directory.
                firstDataBlock /= MEMORY_CARD_SECTORS_PER_BLOCK;
                firstDataBlock -= 1;
                for (fileBlockIndex = 0; fileBlockIndex < fileBlockCount; fileBlockIndex++) {
                    work->blockOwners[firstDataBlock + fileBlockIndex] = MEMORY_CARD_BLOCK_FOREIGN;
                }
                work->foreignBlockCount += fileBlockCount;
            }
        }
        task->state += 1;
    }

    _mcDrawPrompt(task, work->promptId);
}

/// Enumerate this product's saves, restore the save destination and prepare preview reads.
///
/// Requires the preceding wildcard scan's block count and ownership map. Each
/// product file occupies one block; the SDK fills at most fifteen entries.
/// A selective write remembers the current filename's directory index, falling
/// back to zero; an all-section write starts at zero. Foreign files filling all
/// blocks show the card-full prompt; otherwise preview enumeration or selection
/// follows. The SDK return is ignored and the current prompt is drawn.
static void _mcStateReadSaveDirectory(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_FILENAME_BYTES         = 20,
        MEMORY_CARD_SAVE_STATE_CARD_FULL   = 0x19,
        MEMORY_CARD_SAVE_STATE_SELECT_FILE = 0x26,
    };
    s32 candidateDirectoryIndex;
    s32 foreignBlockCount;
    s32 entryCount;
    s32 nextState;

    work->entryCount = 0;
    MemCardGetDirentry(
        work->channel, (char*)Mc_SaveFilePattern, work->directory, &work->entryCount, 0,
        ARRAY_SIZE(work->directory));
    // This game's files count as one block each, so what remains belongs to other products.
    foreignBlockCount       = work->foreignBlockCount - work->entryCount;
    work->foreignBlockCount = foreignBlockCount;
    if (foreignBlockCount == MEMORY_CARD_BLOCK_COUNT) {
        nextState = MEMORY_CARD_SAVE_STATE_CARD_FULL;
    } else {
        if (work->slotWriteMask == MEMORY_CARD_SLOT_WRITE_ALL) {
            work->selectedSlot = 0;
        } else {
            entryCount         = work->entryCount;
            work->selectedSlot = 0;
            if (entryCount != 0) {
                candidateDirectoryIndex = 0;
                if (entryCount > 0) {
                    do {
                        if (strncmp(work->directory[candidateDirectoryIndex].name, (char*)Mc_FileName, MEMORY_CARD_FILENAME_BYTES) == 0) {
                            work->selectedSlot = candidateDirectoryIndex;
                            break;
                        }
                        entryCount               = work->entryCount;
                        candidateDirectoryIndex += 1;
                    } while (candidateDirectoryIndex < entryCount);
                }
            }
        }
        work->currentSlot = 0;
        if (work->entryCount > 0) {
            nextState = task->state + 1;
        } else {
            nextState = MEMORY_CARD_SAVE_STATE_SELECT_FILE;
        }
    }
    task->state = nextState;

    // Sector heads 64..960 map to indices 0..14, excluding the card directory block.
    if (work->entryCount > 0) {
        s32 directoryIndex;

        for (directoryIndex = 0; directoryIndex < work->entryCount; directoryIndex++) {
            s32 firstDataBlock                = work->directory[directoryIndex].head;
            firstDataBlock                   /= MEMORY_CARD_SECTORS_PER_BLOCK;
            firstDataBlock                   -= 1;
            work->blockOwners[firstDataBlock] = directoryIndex;
        }
    }

    _mcDrawPrompt(task, work->promptId);
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

/// Save the current card filename, or restore the remembered one.
///
/// Zero saves and any nonzero value restores. Copies the 20-byte card name
/// and its terminator; the last three bytes of each 24-byte buffer are retained.
static inline void _mcCopyFileName(s32 restoreSavedName)
{
    enum { MEMORY_CARD_FILENAME_COPY_BYTES = 21 };
    const u8* sourceName;
    u8*       destinationName;
    s32       filenameByteIndex;

    if (restoreSavedName == 0) {
        sourceName      = Mc_FileName;
        destinationName = Mc_FileNameBuf;
    } else {
        sourceName      = Mc_FileNameBuf;
        destinationName = Mc_FileName;
    }

    for (filenameByteIndex = 0; filenameByteIndex < MEMORY_CARD_FILENAME_COPY_BYTES; filenameByteIndex++) {
        *destinationName++ = *sourceName++;
    }
}

/// Start closing a memory-card file list and restore its parent dialog's input.
///
/// Called after the parent consumes either a selected row or Cancel. Borrows a
/// live list node and owner, and the writable panel of its parent dialog.
/// Disables list input before detaching and starting its subtree's closing
/// animations. Dialog input resumes immediately; UI task updates release the
/// closing objects later under `uiStartTreeClosing`'s lifetime contract.
static inline void _mcCloseFileList(UiObject* listObject, UiPanel* dialogPanel)
{
    listObject->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    uiStartTreeClosing(listObject, listObject->owner);
    dialogPanel->control.word = USER_INTERFACE_PANEL_ACTIVE;
}

/// Choose an existing save destination or the New Block row while probing the card.
///
/// Borrows a live dialog object and the cached directory/preview work until its
/// list child closes. The directory has 0..15 files; New Block is offered at
/// `entryCount` only when an unoccupied card block remains. Restores the remembered
/// save cursor. A changed destination forces every section to be written; Cancel
/// returns to the save question. A confirmed choice wins over card polling that run.
static void _mcStateSelectSaveFile(Task* dialogTask, McWork* dialogWork)
{
    UiObject* dialogObject;
    UiObject* listObject;
    Task*     listTask;
    s32       syncState;
    s32       filenameByteIndex;

    dialogObject         = dialogTask->spawnArg2.pointer;
    dialogWork->promptId = MEMORY_CARD_PROMPT_SELECT;
    _mcDrawPrompt(dialogTask, MEMORY_CARD_PROMPT_SELECT);

    listTask = dialogTask->firstChild;
    if (listTask == NULL) {
        if (uiSpawnObject(Mc_LoadListDescriptors, dialogWork, USER_INTERFACE_PANEL_ACTIVE,
                          MEMORY_CARD_FILE_LIST_OPENING_DELAY_TICKS, dialogObject) != NULL) {
            Mc_LoadSlotList.itemCount = dialogWork->entryCount;
            if (dialogWork->entryCount < MEMORY_CARD_BLOCK_COUNT - dialogWork->foreignBlockCount) {
                Mc_LoadSlotList.itemCount++;
            }
            Mc_LoadSlotList.selectedItemIndex = dialogWork->selectedSlot;
            dialogObject->resultValue         = 0;
            dialogObject->panel.control.word  = USER_INTERFACE_PANEL_INACTIVE;
        }
    } else {
        listObject = listTask->spawnArg2.pointer;
        if (listObject->result == USER_INTERFACE_RESULT_CONFIRM) {
            dialogObject->resultValue = listObject->resultValue;
            _mcCloseFileList(listObject, &dialogObject->panel);
            if (dialogObject->resultValue >= 0) {
                if (dialogObject->resultValue < dialogWork->entryCount) {
                    const u8* directoryName;
                    u8*       filenameCursor;
                    s32       differingByteCount;

                    // Remember the old name before replacing the full SDK filename field.
                    directoryName      = (const u8*)dialogWork->directory[dialogObject->resultValue].name;
                    filenameCursor     = Mc_FileName;
                    differingByteCount = sizeof(dialogWork->directory[0].name);
                    _mcCopyFileName(false);
                    for (filenameByteIndex = 0; filenameByteIndex < (s32)sizeof(dialogWork->directory[0].name); filenameByteIndex++) {
                        if (*filenameCursor == *directoryName) {
                            differingByteCount--;
                        }
                        *filenameCursor = *directoryName;
                        filenameCursor++;
                        directoryName++;
                    }
                    *filenameCursor = 0;
                    if (differingByteCount != 0) {
                        // A different file transfers every slot, and the next listing starts at slot 0.
                        dialogWork->slotWriteMask = MEMORY_CARD_SLOT_WRITE_ALL;
                    }
                    dialogWork->promptId = MEMORY_CARD_PROMPT_CHECKING;
                    dialogTask->state    = MEMORY_CARD_SAVE_STATE_BEGIN_OPEN_FILE;
                } else {
                    _mcCopyFileName(false);
                    _mcBuildSaveFileName(Mc_FileName, dialogObject->resultValue);
                    dialogWork->promptId = MEMORY_CARD_PROMPT_CHECKING;
                    dialogTask->state    = MEMORY_CARD_SAVE_STATE_BEGIN_OPEN_FILE;
                }
            } else {
                dialogTask->state = MEMORY_CARD_SAVE_STATE_DELAY_CONFIRMATION;
            }
            return;
        }
    }

    // A completed presence error closes the list before restarting card acceptance.
    syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &dialogWork->syncCommand, &dialogWork->syncResult);
    if (syncState != MEMORY_CARD_SYNC_IDLE) {
        if (syncState == MEMORY_CARD_SYNC_COMPLETE && dialogWork->syncResult != McErrNone) {
            dialogTask->state = MEMORY_CARD_SAVE_STATE_ACCEPT_CARD;
            _mcCloseChildUi(dialogTask, syncState);
        }
    } else {
        MemCardExist(dialogWork->channel);
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

/// Resolve save-card acceptance and choose which resident sections need writing.
///
/// A successful probe compares live records with backups; a changed card
/// invalidates both remembered filename suffixes and selects all sections.
/// Both paths reset the nine-section walk, require overwrite confirmation and
/// begin directory enumeration. Missing, unformatted and failed cards enter
/// their recovery states. Draws the current prompt on the live dialog object.
static void _mcStateResolveSaveCardProbe(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_SAVE_STATE_UNFORMATTED     = 0x15,
        MEMORY_CARD_SAVE_STATE_BEGIN_DIRECTORY = 0x1F,
    };
    s32 writeMask;
    u32 probeResult;

    probeResult = work->syncResult;
    switch (probeResult) {
        case McErrNone:
            work->slotsRemaining   = MEMORY_CARD_BUFFER_SLOT_COUNT;
            writeMask              = _mcGetSectionWriteMask();
            work->confirmOverwrite = MEMORY_CARD_OVERWRITE_CONFIRM;
            work->slotWriteMask    = writeMask;
            task->state            = MEMORY_CARD_SAVE_STATE_BEGIN_DIRECTORY;
            break;
        case McErrNewCard:
            _mcInvalidateFileNameSuffixes();
            work->slotsRemaining   = MEMORY_CARD_BUFFER_SLOT_COUNT;
            work->slotWriteMask    = MEMORY_CARD_SLOT_WRITE_ALL;
            work->confirmOverwrite = MEMORY_CARD_OVERWRITE_CONFIRM;
            task->state            = MEMORY_CARD_SAVE_STATE_BEGIN_DIRECTORY;
            break;
        case McErrCardNotExist:
            task->state = MEMORY_CARD_SAVE_STATE_NO_CARD;
            break;
        case McErrNotFormat:
            task->state = MEMORY_CARD_SAVE_STATE_UNFORMATTED;
            break;
        case McErrCardInvalid:
        default:
            task->state = MEMORY_CARD_SAVE_STATE_ACCESS_FAILED;
            break;
    }

    _mcDrawPrompt(task, work->promptId);
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

/// Create a one-block save file when the card settle timer reaches zero.
///
/// Entry requires a positive timer and the current product filename. Stores
/// the SDK result and selects header writing, card recovery, formatting,
/// card-full or save-failed handling. Draws the current prompt while waiting.
static void _mcStateCreateSaveFile(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_SAVE_FILE_BLOCKS              = 1,
        MEMORY_CARD_SAVE_STATE_BEGIN_HEADER_WRITE = 0xA,
        MEMORY_CARD_SAVE_STATE_CONFIRM_FORMAT     = 0x15,
        MEMORY_CARD_SAVE_STATE_CARD_FULL          = 0x19
    };
    u32 createResult;

    work->cardTimer -= 1;
    if (work->cardTimer == 0) {
        createResult     = MemCardCreateFile(work->channel, (char*)Mc_FileName, MEMORY_CARD_SAVE_FILE_BLOCKS);
        work->syncResult = createResult;
        switch (createResult) {
            case McErrNone:
                task->state = MEMORY_CARD_SAVE_STATE_BEGIN_HEADER_WRITE;
                break;
            case McErrCardNotExist:
                task->state = MEMORY_CARD_SAVE_STATE_NO_CARD;
                break;
            case McErrNotFormat:
                task->state = MEMORY_CARD_SAVE_STATE_CONFIRM_FORMAT;
                break;
            case McErrBlockFull:
                task->state = MEMORY_CARD_SAVE_STATE_CARD_FULL;
                break;
            case McErrCardInvalid:
            case McErrNewCard:
            case McErrFileNotExist:
            case McErrAlreadyExist:
            default:
                task->state = MEMORY_CARD_SAVE_STATE_WRITE_FAILED;
                break;
        }
    }
    _mcDrawPrompt(task, work->promptId);
}

/// Resolve the initial file-header write before reopening the new file for saving.
///
/// A successful sync restarts all nine sections without overwrite confirmation.
/// Every other result invalidates both filename suffixes and shows save failure.
/// The preceding header transfer uses resident bytes, so clearing `buffer` owns
/// no allocation. Draws the current prompt on the live dialog object.
static void _mcStateResolveFileHeaderWrite(Task* task, McWork* work)
{
    /// Invalidate both filename suffixes and enter the save-failure state.
    ///
    /// The cursor arguments must be distinct, simple writable u8* local lvalues
    /// pointing at buffers of at least 21 bytes. They are evaluated repeatedly and
    /// advanced to byte 20, which is terminated; later backing bytes are retained.
    /// `dialogTask` is evaluated once and must point at the live save dialog task.
#define MEMORY_CARD_FAIL_FILE_HEADER_WRITE(dialogTask, filenameCursor, savedFilenameCursor) \
    do {                                                                                    \
        enum {                                                                              \
            MEMORY_CARD_FILENAME_PREFIX_BYTES = 12,                                         \
            MEMORY_CARD_FILENAME_BYTES        = 20                                          \
        };                                                                                  \
        s32 filenameByteIndex = 0;                                                          \
        s32 unusedChar        = '_';                                                        \
        do {                                                                                \
            if (filenameByteIndex >= MEMORY_CARD_FILENAME_PREFIX_BYTES) {                   \
                *(savedFilenameCursor) = unusedChar;                                        \
                *(filenameCursor)      = unusedChar;                                        \
            }                                                                               \
            (filenameCursor)++;                                                             \
            filenameByteIndex++;                                                            \
            (savedFilenameCursor)++;                                                        \
        } while (filenameByteIndex < MEMORY_CARD_FILENAME_BYTES);                           \
        *(savedFilenameCursor) = 0;                                                         \
        *(filenameCursor)      = 0;                                                         \
        (dialogTask)->state    = MEMORY_CARD_SAVE_STATE_WRITE_FAILED;                       \
    } while (0)
    u32 writeResult;
    u8* filenameByte;
    u8* savedFilenameByte;

    writeResult = work->syncResult;
    if (writeResult < (u32)McErrNotFormat) {
        filenameByte = Mc_FileName;
        if (writeResult == McErrNone) {
            work->slotsRemaining   = MEMORY_CARD_BUFFER_SLOT_COUNT;
            work->slotWriteMask    = MEMORY_CARD_SLOT_WRITE_ALL;
            work->confirmOverwrite = MEMORY_CARD_OVERWRITE_PROCEED;
            task->state            = MEMORY_CARD_SAVE_STATE_BEGIN_OPEN_FILE;
        } else {
            goto invalidateFileNames;
        }
    } else {
        filenameByte = Mc_FileName;
    invalidateFileNames:
        savedFilenameByte = Mc_FileNameBuf;
        MEMORY_CARD_FAIL_FILE_HEADER_WRITE(task, filenameByte, savedFilenameByte);
    }
    work->buffer = NULL;

    _mcDrawPrompt(task, work->promptId);
#undef MEMORY_CARD_FAIL_FILE_HEADER_WRITE
}

/// Confirm overwriting the selected file, or start saving when confirmation is waived.
///
/// The overwrite question initially selects No. Yes arms the section-write delay;
/// No restores the remembered filename and arms a retry. Polls card presence after
/// processing either answer, so a completed card error takes priority and returns
/// to card acceptance. Both the dialog and any first choice child must be live.
static void _mcStateConfirmSaveOverwrite(Task* task, McWork* work)
{
    s32 syncState;

    if (work->confirmOverwrite == MEMORY_CARD_OVERWRITE_CONFIRM) {
        work->promptId = MEMORY_CARD_PROMPT_OVERWRITE;
        switch (_mcUpdateYesNoPromptInitialNo(task, MEMORY_CARD_PROMPT_OVERWRITE, work->promptTimer)) {
            case MEMORY_CARD_PROMPT_ANSWER_PENDING:
                break;
            case MEMORY_CARD_PROMPT_ANSWER_YES:
                work->cardTimer    = MEMORY_CARD_IO_SETTLE_FRAMES;
                work->sectorOffset = 0;
                task->state        = MEMORY_CARD_SAVE_STATE_DELAY_SECTION;
                break;
            case MEMORY_CARD_PROMPT_ANSWER_NO:
                _mcCopyFileName(true);
                task->killCountdown = MEMORY_CARD_SAVE_RETRY_DELAY_FRAMES;
                task->state         = MEMORY_CARD_SAVE_STATE_DELAY_RETRY;
                break;
        }
        syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &work->syncCommand, &work->syncResult);
        if (syncState != MEMORY_CARD_SYNC_IDLE) {
            if (syncState == MEMORY_CARD_SYNC_COMPLETE && work->syncResult != McErrNone) {
                task->state = MEMORY_CARD_SAVE_STATE_ACCEPT_CARD;
                _mcCloseChildUi(task, syncState);
            }
        } else {
            MemCardExist(work->channel);
        }
    } else {
        work->sectorOffset = 0;
        work->promptId     = MEMORY_CARD_PROMPT_SAVING;
        task->state        = MEMORY_CARD_SAVE_STATE_PREPARE_SECTION;
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

/// Prepare the next selected file section, or refresh resident backups when the walk ends.
///
/// Entry requires `slotsRemaining` in 0..9 and no owned transfer allocation.
/// Its section index is `9 - slotsRemaining` before decrementing. Header section
/// zero copies its two distinct halves; record sections copy the newly checksummed
/// live record twice. Transfers round the two-copy byte length up to 128 bytes,
/// with a zero-filled tail, and remain owned until write completion. Allocation
/// failure retries; a clear mask bit skips the section's sector extent.
static void _mcStatePrepareSectionWrite(Task* task, McWork* work)
{
    enum { MEMORY_CARD_CURRENT_SECTION_SELECTED = 1 << 0 };
    u8* liveRecordBytes;
    s32 bytesPerCopy;
    u8* transferBuffer;

    if (work->slotsRemaining == 0) {
        _mcBackupSaveSections();
        work->closeAnswer = USER_INTERFACE_LIST_COMMAND_YES;
        task->state       = MEMORY_CARD_SAVE_STATE_CLOSE_PROMPT;
    } else if (work->slotWriteMask & MEMORY_CARD_CURRENT_SECTION_SELECTED) {
        bytesPerCopy        = Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - work->slotsRemaining].bytesPerCopy;
        liveRecordBytes     = Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - work->slotsRemaining].buffer;
        work->transferBytes = (((u32)(bytesPerCopy * 2 - 1) >> MEMORY_CARD_SECTOR_BYTE_SHIFT) + 1) << MEMORY_CARD_SECTOR_BYTE_SHIFT;
        transferBuffer      = memCalloc(work->transferBytes, false);
        if (transferBuffer != NULL) {
            work->buffer = transferBuffer;
            if (work->slotsRemaining == MEMORY_CARD_BUFFER_SLOT_COUNT) {
                // The file header precedes the save record containing its checksum.
                _mcBuildCardFileTitle(work);
                memcpy(transferBuffer, liveRecordBytes, bytesPerCopy * 2);
            } else {
                _mcWriteBlockChecksum(liveRecordBytes, bytesPerCopy);
                if (work->slotsRemaining == MEMORY_CARD_BUFFER_SLOT_COUNT - 1) {
                    _mcWriteSectionChecksumSummary();
                }
                // Both on-card copies come from the live record, not its old backup.
                memcpy(transferBuffer, liveRecordBytes, bytesPerCopy);
                memcpy(transferBuffer + bytesPerCopy, liveRecordBytes, bytesPerCopy);
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

/// Format the card after its settle delay, then prepare a fresh save filename.
///
/// Entry requires a positive frame timer and a live dialog object. The SDK
/// format call completes synchronously: success empties the cached directory
/// and enters file creation, no card enters insertion recovery, and every
/// other result shows format failure. Draws the current prompt while waiting.
static void _mcStateFormatCard(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_SAVE_STATE_BEGIN_CREATE  = 8,
        MEMORY_CARD_SAVE_STATE_FORMAT_FAILED = 0x2B,
    };
    s32 formatResult;
    s32 nextState;

    work->cardTimer -= 1;
    if (work->cardTimer == 0) {
        formatResult     = MemCardFormat(work->channel);
        work->syncResult = formatResult;
        if (formatResult != McErrCardNotExist) {
            if (formatResult != McErrNone) {
                nextState = MEMORY_CARD_SAVE_STATE_FORMAT_FAILED;
            } else {
                _mcBuildSaveFileName(Mc_FileName, 0);
                nextState        = MEMORY_CARD_SAVE_STATE_BEGIN_CREATE;
                work->entryCount = 0;
            }
        } else {
            nextState = MEMORY_CARD_SAVE_STATE_NO_CARD;
        }
        task->state = nextState;
    }

    _mcDrawPrompt(task, work->promptId);
}

/// Choose a cached save file for loading, restarting acceptance on a card-probe error.
///
/// Borrows a live dialog object and directory/preview work until the list child
/// closes. Entry requires 1..15 files; the row callback accepts only a valid
/// preview and publishes an index below `entryCount`, or -1 for Cancel. A probe
/// error takes priority over the list result. Selection copies twenty filename
/// bytes plus NUL and enters the section-load delay; Cancel closes the load file.
static void _mcStateSelectLoadFile(Task* dialogTask, McWork* dialogWork)
{
    enum {
        MEMORY_CARD_LOAD_STATE_ACCEPT_CARD   = 7,
        MEMORY_CARD_LOAD_STATE_BEGIN_SECTION = 0xC,
    };
    UiObject* dialogObject;
    s32       syncState;
    s32       filenameByteIndex;
    Task*     listTask;
    UiObject* listObject;
    const u8* directoryName;
    u8*       filenameCursor;

    dialogObject         = dialogTask->spawnArg2.pointer;
    dialogWork->promptId = MEMORY_CARD_PROMPT_SELECT;
    _mcDrawPrompt(dialogTask, MEMORY_CARD_PROMPT_SELECT);

    syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &dialogWork->syncCommand, &dialogWork->syncResult);
    if (syncState == MEMORY_CARD_SYNC_IDLE) {
        MemCardExist(dialogWork->channel);
    } else if (syncState == MEMORY_CARD_SYNC_COMPLETE && dialogWork->syncResult != McErrNone) {
        dialogTask->state = MEMORY_CARD_LOAD_STATE_ACCEPT_CARD;
        _mcCloseChildUi(dialogTask, syncState);
        return;
    }
    listTask = dialogTask->firstChild;
    if (listTask == NULL) {
        if (uiSpawnObject(Mc_SaveListDesc, dialogWork, USER_INTERFACE_PANEL_ACTIVE,
                          MEMORY_CARD_FILE_LIST_OPENING_DELAY_TICKS, dialogObject) != NULL) {
            Mc_SaveSlotList.itemCount        = dialogWork->entryCount;
            dialogObject->resultValue        = 0;
            dialogObject->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    } else {
        listObject = listTask->spawnArg2.pointer;
        if (listObject->result == USER_INTERFACE_RESULT_CONFIRM) {
            dialogObject->resultValue = listObject->resultValue;
            _mcCloseFileList(listObject, &dialogObject->panel);
            if (dialogObject->resultValue >= 0) {
                directoryName  = (const u8*)dialogWork->directory[dialogObject->resultValue].name;
                filenameCursor = Mc_FileName;
                for (filenameByteIndex = 0; filenameByteIndex < (s32)sizeof(dialogWork->directory[0].name); filenameByteIndex++) {
                    *filenameCursor++ = *directoryName++;
                }
                *filenameCursor   = 0;
                dialogTask->state = MEMORY_CARD_LOAD_STATE_BEGIN_SECTION;
            } else {
                dialogTask->state = MEMORY_CARD_LOAD_STATE_CLOSE_FILE;
            }
        }
    }
}

/// Jump table of 26 _McStateFunc handlers used by Mc_DispatchStateTable26.
static const _McFileSelectStateTable Mc_FileSelectStates = { {
    _mcStateInitLoadWork,
    _mcStateInitLoadSections,
    _mcStateConfirmLoadAfterDelay,
    _mcStateCloseLoadFile,
    _mcStateDismissLoadPrompt,
    _mcStateKillLoadDialogIfRequested,
    _mcStateAcknowledgeLoadFailure,
    _mcStateAcceptLoadCard,
    _mcStatePollCardAdvance,
    _mcStateResolveLoadCardProbe,
    _mcStateWaitLoadCardInsertion,
    _mcStateWaitNoDataCardRemoval,
    _mcStateBeginSectionLoad,
    _mcStateOpenSaveFileForRead,
    _mcStatePrepareSectionRead,
    _mcStateReadSection,
    _mcStatePollCardAdvance,
    _mcStateFinishSectionRead,
    _mcStateBeginLoadDirectory,
    _mcStateReadLoadDirectory,
    _mcStateOpenLoadPreview,
    _mcStateReadLoadPreview,
    _mcStatePollCardAdvance,
    _mcStateAdvanceLoadPreview,
    _mcStateSelectLoadFile,
    _mcStateAcknowledgeCorruptLoad,
} };

/// Resolve the load card probe, invalidating remembered names before enumeration.
///
/// Success or a changed card clears only the filename suffixes and enters
/// directory setup. Missing or unformatted cards show the corresponding
/// recovery prompt; other results show load failure. Draws the current prompt.
static void _mcStateResolveLoadCardProbe(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_LOAD_STATE_BEGIN_DIRECTORY = 0x12,
        MEMORY_CARD_LOAD_STATE_NO_CARD         = 0xA
    };
    u32 probeResult;

    probeResult = work->syncResult;
    switch (probeResult) {
        case McErrNone:
        case McErrNewCard:
            _mcInvalidateFileNameSuffixes();
            task->state = MEMORY_CARD_LOAD_STATE_BEGIN_DIRECTORY;
            break;
        case McErrCardNotExist:
            task->state = MEMORY_CARD_LOAD_STATE_NO_CARD;
            break;
        case McErrNotFormat:
            task->state = MEMORY_CARD_LOAD_STATE_NO_DATA;
            break;
        case McErrCardInvalid:
        default:
            task->state = MEMORY_CARD_LOAD_STATE_FAILED;
            break;
    }
    _mcDrawPrompt(task, work->promptId);
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

/// Allocate the next selected section read, or validate the restored live records.
///
/// Entry requires `slotsRemaining` in 0..9 and no owned transfer buffer. Before
/// decrementing, `9 - slotsRemaining` indexes the section. Reads both copies,
/// rounded up to a 128-byte sector; completion owns the allocation until it frees
/// it. Allocation failure retries, and a clear mask bit skips the section's
/// sector extent. At the end, valid live checksums arm player-position restoration
/// and close the file; a mismatch resets save data and shows the corruption notice.
static void _mcStatePrepareSectionRead(Task* dialogTask, McWork* dialogWork)
{
    enum {
        MEMORY_CARD_CURRENT_SECTION_SELECTED = 1 << 0,
        MEMORY_CARD_LOAD_STATE_CORRUPTED     = 0x19,
    };
    s32   transferBytes;
    void* transferBuffer;

    if (dialogWork->slotsRemaining == 0) {
        if (_mcVerifySaveSectionChecksums() && _mcVerifySectionChecksumSummary()) {
            playClockResetMinuteTicks();
            gDisplayState.control.flags.pendingPlayerPos = 1;
            dialogTask->state                            = MEMORY_CARD_LOAD_STATE_CLOSE_FILE;
        } else {
            mcResetSaveData();
            dialogTask->state = MEMORY_CARD_LOAD_STATE_CORRUPTED;
        }
    } else if (dialogWork->slotWriteMask & MEMORY_CARD_CURRENT_SECTION_SELECTED) {
        // Preserve unsigned sector rounding of the two-copy byte extent.
        transferBytes             = Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - dialogWork->slotsRemaining].bytesPerCopy;
        transferBytes           <<= 1;
        transferBytes            -= 1;
        transferBytes             = (u32)transferBytes >> MEMORY_CARD_SECTOR_BYTE_SHIFT;
        transferBytes            += 1;
        transferBytes           <<= MEMORY_CARD_SECTOR_BYTE_SHIFT;
        dialogWork->transferBytes = transferBytes;
        transferBuffer            = memMalloc(transferBytes, false);
        dialogWork->buffer        = transferBuffer;
        if (transferBuffer != NULL) {
            dialogWork->cardTimer      = 0;
            dialogTask->state          = dialogTask->state + 1;
            dialogWork->slotsRemaining = dialogWork->slotsRemaining - 1;
            dialogWork->slotWriteMask  = (u32)dialogWork->slotWriteMask >> 1;
        } else {
            dialogWork->cardTimer = dialogWork->cardTimer + 1;
        }
    } else {
        dialogWork->sectorOffset   = dialogWork->sectorOffset + Mc_BufferSlots[MEMORY_CARD_BUFFER_SLOT_COUNT - dialogWork->slotsRemaining].cardSectors;
        dialogWork->slotsRemaining = dialogWork->slotsRemaining - 1;
        dialogWork->slotWriteMask  = (u32)dialogWork->slotWriteMask >> 1;
    }

    dialogWork->promptId = MEMORY_CARD_PROMPT_LOADING;
    _mcDrawPrompt(dialogTask, MEMORY_CARD_PROMPT_LOADING);
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

/// Draw a load-file row and accept only a checksummed preview on Confirm.
///
/// Borrows the list and object, whose owner holds the shared `McWork` in its
/// first spawn argument. The current index must be below `entryCount` (0..14).
/// Newly pressed Confirm on port zero wins over Cancel only for a valid
/// preview; Cancel remains available for a corrupt row and publishes -1.
static void _mcDrawLoadFileRow(UiList* list, UiObject* object)
{
    enum { MEMORY_CARD_FILE_SELECTION_CANCELLED = -1 };
    McWork*              work;
    s32                  directoryIndex;
    const McSavePreview* preview;
    s32                  previewValid;

    previewValid   = 1;
    directoryIndex = list->currentItemIndex;
    work           = object->owner->spawnArg1.pointer;
    preview        = work->previews + directoryIndex;
    if (!_mcVerifySavePreviewChecksum(preview)) {
        previewValid = 0;
        uiGetTextColor(object, USER_INTERFACE_TEXT_COLOR_DIMMED);
    }
    mcDrawFilePreview(object, work, list->currentItemIndex, 0, list->rowTextY.signedValue + 7);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (previewValid && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm)) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
            object->resultValue = list->currentItemIndex;
        } else if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel)) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
            object->resultValue = MEMORY_CARD_FILE_SELECTION_CANCELLED;
        }
    }
}

/// Initialize a preview text request in draw-environment pixels and packed RGB.
///
/// `lineRequest` must be a side-effect-free request lvalue: it is evaluated seven
/// times. Other arguments are evaluated once, in field-store order; coordinates
/// narrow to signed halfwords and selectors to signed bytes. The renderer
/// initializes vertical bias. Expands to a compound statement; use as a
/// standalone statement in a braced body. Defined only around `mcDrawFilePreview`.
#define MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(lineRequest, penX, penY, lineOtIndex, textColorRgb, face, alignmentMode, textDrawMode) \
    {                                                                                                                                \
        (lineRequest).x          = (penX);                                                                                           \
        (lineRequest).y          = (penY);                                                                                           \
        (lineRequest).otIndex    = (lineOtIndex);                                                                                    \
        (lineRequest).colorRgb   = (textColorRgb);                                                                                   \
        (lineRequest).glyphTable = (face);                                                                                           \
        (lineRequest).alignment  = (alignmentMode);                                                                                  \
        (lineRequest).drawMode   = (textDrawMode);                                                                                   \
    }

void mcDrawFilePreview(UiObject* panelObject, const McWork* work, s32 directoryIndex, s32 originX, s32 originY)
{
    enum { MEMORY_CARD_PREVIEW_LABEL_RGB     = 0x606060,
           MEMORY_CARD_CORRUPT_MULTILINE_RGB = 0x37A78 };
    union {
        u8          text[0x20]; // Numeric formatting while drawing an existing preview.
        TextDrawReq request;    // New Block request when no preview exists.
    } numberScratch;
    TextDrawReq timeLabelRequest;
    TextDrawReq timeValueRequest;
    union {
        u8          text[0x10]; // Parenthesized save label, after CLEAR has been drawn.
        TextDrawReq request;    // CLEAR label request before the storage becomes text.
    } saveLabelScratch;
    TextDrawReq          detailRequest;
    TextDrawReq          statRequest;
    TextDrawReq          bpValueRequest;
    s32                  x;
    s32                  y;
    s32                  textX;
    u32                  textColorRgb;
    const McSavePreview* preview;

    textColorRgb = uiGetTextColor(panelObject, USER_INTERFACE_TEXT_COLOR_NORMAL);
    if (directoryIndex < work->entryCount) {
        preview = &work->previews[directoryIndex];
        if (!_mcVerifySavePreviewChecksum(preview)) {
            x = originX + panelObject->panel.contentLeft.signedValue + 8;
            y = originY + panelObject->panel.contentTop.signedValue + 0x11;
            if (work->corruptNoticeStyle == MEMORY_CARD_CORRUPT_NOTICE_PROMPT) {
                textDrawUiLine(panelObject, x, y, McText_LoadAbortedCorrupted, MEMORY_CARD_PREVIEW_LABEL_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
                return;
            }
            textDrawUiLines(panelObject, x, y, McText_LoadAbortedCorrupted, MEMORY_CARD_CORRUPT_MULTILINE_RGB, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
            return;
        }
        // Draw the time and completed-run details around the preview footer.
        x = originX + panelObject->panel.contentLeft.signedValue + 2;
        y = (originY + panelObject->panel.contentBottom.signedValue) - 0x10;
        MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(timeLabelRequest, panelObject->panel.contentOriginX.unsignedValue + x, panelObject->panel.contentOriginY.unsignedValue + (y - 2), panelObject->panel.otIndex.signedValue + 1, MEMORY_CARD_PREVIEW_LABEL_RGB, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
        textDrawString(&timeLabelRequest, (const u8*)McText_Time);
        MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(timeValueRequest, panelObject->panel.contentOriginX.unsignedValue + 0x28 + x, panelObject->panel.contentOriginY.unsignedValue + y, panelObject->panel.otIndex.signedValue + 1, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&timeValueRequest, textFormatPlayTime(numberScratch.text, preview->playTime));
        if (preview->clearCount > 0) {
            x = (originX + panelObject->panel.contentRight.signedValue) - 4;
            y = (originY + panelObject->panel.contentBottom.signedValue) - 0xB;
            MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(saveLabelScratch.request, panelObject->panel.contentOriginX.unsignedValue + (x - 0x1E), panelObject->panel.contentOriginY.unsignedValue + (y - 2), panelObject->panel.otIndex.signedValue + 1, MEMORY_CARD_PREVIEW_LABEL_RGB, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
            textDrawString(&saveLabelScratch.request, (const u8*)McText_Clear);
            MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(detailRequest, panelObject->panel.contentOriginX.unsignedValue + x, panelObject->panel.contentOriginY.unsignedValue + y, panelObject->panel.otIndex.signedValue + 1, textColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
            textDrawString(&detailRequest, textItoaUnsigned(numberScratch.text, preview->clearCount));
            if ((s8)preview->savePoint != MEMORY_CARD_SAVE_POINT_OPENING) {
                MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(statRequest, panelObject->panel.contentOriginX.unsignedValue + x, panelObject->panel.contentOriginY.unsignedValue + 8 + y, panelObject->panel.otIndex.signedValue + 1, MEMORY_CARD_PREVIEW_LABEL_RGB, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
                textDrawString(&statRequest, Mc_ModeLabels[preview->gameMode]);
            }
        }
        x = originX + panelObject->panel.contentLeft.signedValue + 4;
        y = originY + panelObject->panel.contentTop.signedValue + 0x11;
        textDrawUiLine(panelObject, x, y, Mc_LocationLabels[(s8)preview->savePoint], textColorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
        // Reuse the CLEAR request storage for the parenthesized save number.
        saveLabelScratch.text[0] = 0;
        textAppendString(saveLabelScratch.text, (const u8*)McText_OpenParen);
        textAppendString(saveLabelScratch.text, textItoaSigned(numberScratch.text, preview->saveNumber));
        textAppendString(saveLabelScratch.text, (const u8*)McText_CloseParen);
        MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(detailRequest, panelObject->panel.contentOriginX.unsignedValue + (x + textMeasureLineWidth(Mc_LocationLabels[(s8)preview->savePoint])), panelObject->panel.contentOriginY.unsignedValue + (y - 3), panelObject->panel.otIndex.signedValue + 1, textColorRgb, TEXT_GLYPH_TABLE_LARGE, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
        textDrawString(&detailRequest, saveLabelScratch.text);
        textX                    = originX + panelObject->panel.contentLeft.signedValue;
        x                        = textX + 2;
        y                        = (originY + panelObject->panel.contentBottom.signedValue) - 1;
        detailRequest.x          = panelObject->panel.contentOriginX.unsignedValue + x;
        x                       += 0x28;
        detailRequest.y          = panelObject->panel.contentOriginY.unsignedValue + (y - 2);
        detailRequest.otIndex    = panelObject->panel.otIndex.signedValue + 1;
        detailRequest.colorRgb   = MEMORY_CARD_PREVIEW_LABEL_RGB;
        detailRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        detailRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        detailRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&detailRequest, (const u8*)McText_Exp);
        if ((s8)preview->savePoint != MEMORY_CARD_SAVE_POINT_OPENING) {
            MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(statRequest, panelObject->panel.contentOriginX.unsignedValue + x, panelObject->panel.contentOriginY.unsignedValue + y, panelObject->panel.otIndex.signedValue + 1, MEMORY_CARD_PREVIEW_LABEL_RGB, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
            textDrawString(&statRequest, textItoaSigned(numberScratch.text, preview->playerExp));
        } else {
            MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(statRequest, panelObject->panel.contentOriginX.unsignedValue + x, panelObject->panel.contentOriginY.unsignedValue + y, panelObject->panel.otIndex.signedValue + 1, MEMORY_CARD_PREVIEW_LABEL_RGB, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
            textDrawString(&statRequest, (const u8*)McText_Unavailable);
        }
        x                      = originX - 0x28;
        statRequest.x          = panelObject->panel.contentOriginX.unsignedValue + x;
        statRequest.y          = panelObject->panel.contentOriginY.unsignedValue + (y - 2);
        statRequest.otIndex    = panelObject->panel.otIndex.signedValue + 1;
        statRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        statRequest.colorRgb   = MEMORY_CARD_PREVIEW_LABEL_RGB;
        statRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        statRequest.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&statRequest, (const u8*)McText_Bp);
        if ((s8)preview->savePoint != MEMORY_CARD_SAVE_POINT_OPENING) {
            MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(bpValueRequest, panelObject->panel.contentOriginX.unsignedValue + 0x1E + x, panelObject->panel.contentOriginY.unsignedValue + y, panelObject->panel.otIndex.signedValue + 1, MEMORY_CARD_PREVIEW_LABEL_RGB, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
            textDrawString(&bpValueRequest, textItoaSigned(numberScratch.text, preview->playerBp));
        } else {
            MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST(bpValueRequest, panelObject->panel.contentOriginX.unsignedValue + 0x1E + x, panelObject->panel.contentOriginY.unsignedValue + y, panelObject->panel.otIndex.signedValue + 1, MEMORY_CARD_PREVIEW_LABEL_RGB, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
            textDrawString(&bpValueRequest, (const u8*)McText_Unavailable);
        }
    } else {
        // A free card block has no cached preview to read.
        numberScratch.request.x          = panelObject->panel.contentOriginX.unsignedValue + originX;
        numberScratch.request.y          = panelObject->panel.contentOriginY.unsignedValue + 5 + originY;
        numberScratch.request.otIndex    = panelObject->panel.otIndex.signedValue + 1;
        numberScratch.request.glyphTable = TEXT_GLYPH_TABLE_LARGE;
        numberScratch.request.colorRgb   = textColorRgb;
        numberScratch.request.alignment  = TEXT_ALIGNMENT_CENTER;
        numberScratch.request.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&numberScratch.request, McText_NewBlock);
    }
}

#undef MEMORY_CARD_INIT_PREVIEW_TEXT_REQUEST

/// Append ASCII text as full-width Shift-JIS glyphs to a memory-card title.
///
/// The NUL-terminated source is borrowed and read-only: use letters or space
/// through '@'. The halfword-aligned destination must not overlap the source
/// and needs one halfword per character plus a zero halfword. No capacity is
/// checked. Returns the terminator for another append. This unused out-of-line
/// entry point is retained for the image layout.
static u16* _mcEncodeAsciiTitleText(const u8* asciiText, u16* titleCursor)
{
    return _mcAppendAsciiTitleText(asciiText, titleCursor);
}

/// Invalidate the current and remembered card filenames, preserving their prefix.
///
/// Writes underscores to bytes 12..19 and NUL to byte 20 of both 24-byte arrays.
/// Retained unused out-of-line entry point; bytes 0..11 and 21..23 are preserved.
static void _mcResetFileNameSuffixes(void)
{
    _mcInvalidateFileNameSuffixes();
}

/// Save the current card filename, or restore the remembered one.
///
/// Zero saves; any nonzero `restoreSavedName` restores. Copies the 20-byte name
/// and its terminator, preserving the final three bytes of each 24-byte array.
/// Retained unused out-of-line entry point.
static void _mcCopyCardFileName(s32 restoreSavedName)
{
    _mcCopyFileName(restoreSavedName);
}

/// Write the live save's preview checksum and complement.
///
/// Sums 56 signed bytes beginning at `location`, including the checksum pair
/// and the following saved-state bytes. The pair starts at zero and all ones;
/// storing a checksum and its complement preserves its byte-sum contribution
/// of -2. Calls the preview verifier and discards its result. This unused
/// out-of-line entry point is retained for the image layout.
static void _mcWriteLiveSaveHeaderChecksum(void)
{
    _mcWriteSaveHeaderChecksum();
}

/// Check a resident save's preview checksum and its place-label range (1..16).
///
/// Reads the shared prefix through `save->preview`; neither the complete saved
/// state payload nor the card file header is verified. Returns nonzero on success.
static s32 _mcVerifySaveHeaderChecksum(const McSaveData* save)
{
    return _mcVerifySavePreviewChecksum(&save->preview);
}

/// Write a save record's signed-byte payload checksum and its complement.
///
/// `recordBytes` borrows writable, halfword-aligned storage. `recordByteCount`
/// includes the four-byte checksum header and must be at least four. Each
/// addition retains its low 16 bits; an empty payload stores zero and 0xFFFF.
/// Retained unused out-of-line entry point.
static void _mcWriteRecordChecksum(u8* recordBytes, s32 recordByteCount)
{
    _mcWriteBlockChecksum(recordBytes, recordByteCount);
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

/// Return whether a save record's checksum matches its signed-byte payload sum.
///
/// Borrows halfword-aligned, read-only storage of `recordByteCount` bytes,
/// including the four-byte checksum header; the count must be at least four.
/// Each addition retains its low 16 bits. The checksum complement is unchecked.
/// Retained unused out-of-line entry point.
static s32 _mcVerifyRecordChecksum(const u8* recordBytes, s32 recordByteCount)
{
    const _McChecksumBlock* block;
    s16                     sum;
    u32                     byteIndex;

    block            = (const _McChecksumBlock*)recordBytes;
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
    return (block->checksum ^ (sum & MEMORY_CARD_CHECKSUM_MASK)) == 0;
}

/// Retained unused memory-card no-op; its original purpose is unproven.
static void _mcNoOp(void)
{
}

/// Return the nine-bit mask of save sections selected for writing.
///
/// Bit n selects `Mc_BufferSlots[n]`. Sections 1..8 compare their complete live
/// record with the adjacent backup; bits 0, 1 and 8 are always set for the file
/// header, save state and nibble bank. Retained unused out-of-line entry point.
static s32 _mcQuerySectionWriteMask(void)
{
    return _mcGetSectionWriteMask();
}

/// Write the checksum and complement of the payload in each live save section.
///
/// Sections 1..8 sum signed bytes after their four-byte checksum header,
/// retaining the low 16 bits. The card file header and backups are kept intact.
/// This unused out-of-line entry point is retained for the image layout.
static void _mcWriteLiveSaveSectionChecksums(void)
{
    _mcWriteSaveSectionChecksums();
}

/// Store the live sections' low-checksum-byte sum and its complement in the save.
///
/// Adds sections 1..8's unsigned low checksum bytes, yielding 0..2040, to the
/// live saved state's `bufferChecksum` pair. The checksums must already have
/// been computed. The card file header and backups are excluded. This unused
/// out-of-line entry point is retained for the image layout.
static void _mcWriteLiveSectionChecksumSummary(void)
{
    _mcWriteSectionChecksumSummary();
}

/// Return whether the live save matches the sections' low-checksum-byte sum.
///
/// Adds sections 1..8's unsigned low checksum bytes, yielding 0..2040, and
/// compares `bufferChecksum`. Its complement, backups and file header are
/// unchecked. Retained unused out-of-line entry point.
static s32 _mcCheckSectionChecksumSummary(void)
{
    return _mcVerifySectionChecksumSummary();
}

/// Return whether every live save section has the correct payload checksum.
///
/// Sections 1..8 sum signed bytes after their four-byte checksum header,
/// retaining the low 16 bits. Visits every live record even after a mismatch;
/// backups and complements are unchecked. Retained unused out-of-line entry point.
static s32 _mcCheckSaveSectionChecksums(void)
{
    return _mcVerifySaveSectionChecksums();
}

/// Copy sections 1..8's complete live records over their adjacent backups.
///
/// Each copy is `bytesPerCopy` bytes, including its checksum header. Section
/// zero holds distinct card-header data and is excluded. Does not allocate or
/// write to the card. Retained unused out-of-line entry point.
static void _mcBackupLiveSaveSections(void)
{
    _mcBackupSaveSections();
}

/// Draw the memory-card title and both lines of a prompt, clearing the UI result.
///
/// `dialogTask->spawnArg2.pointer` must borrow a live `UiObject`. `promptId`
/// indexes `Mc_PromptTable`; text uses normal color, outlines and left alignment.
/// Retained unused out-of-line entry point.
static void _mcDrawDialogPrompt(Task* dialogTask, s32 promptId)
{
    _mcDrawPrompt(dialogTask, promptId);
}

/// Close the first child UI tree and reactivate its dialog's input.
///
/// Does nothing without a child. Both tasks must borrow live `UiObject`s in
/// `spawnArg2.pointer`. Deactivates the child, detaches and starts closing its
/// tree, then enables dialog input. UI tasks release the closing objects later.
/// Retained unused out-of-line entry point.
static void _mcCloseChildAndReactivateDialog(Task* dialogTask)
{
    _mcCloseChildUi(dialogTask, USER_INTERFACE_PANEL_ACTIVE);
}

/// Write the signed-byte checksum pair for the resident or read card file header.
///
/// Zero `useReadBuffer` selects `Mc_DefaultChecksumSrc` and the live save's
/// `titleChecksum` pair. Any nonzero value selects `work->buffer` and the work's
/// checksum pair; that buffer must contain at least the complete 512-byte header.
/// The source is borrowed for this call; additions retain their low 16 bits.
/// Retained unused out-of-line entry point.
static void _mcWriteSelectedCardHeaderChecksum(s32 useReadBuffer, McWork* work)
{
    s16       sum;
    s32       headerByteCount;
    const u8* headerByte;
    u16*      checksum;
    s32       byteIndex;

    sum             = 0;
    headerByteCount = sizeof(Mc_DefaultChecksumSrc);
    if (useReadBuffer == 0) {
        headerByte = Mc_DefaultChecksumSrc;
        checksum   = &gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.titleChecksum;
    } else {
        headerByte = work->buffer;
        checksum   = &work->checksum;
    }

    // The checksum and complement remain fields of the same selected owner.
    byteIndex = 0;
    *checksum = sum;

    *(useReadBuffer == 0 ? &PARENT_OF(checksum, McSaveState, titleChecksum)->titleChecksumComplement : &PARENT_OF(checksum, McWork, checksum)->checksumComplement) = ~sum;
    while (byteIndex < headerByteCount) {
        byteIndex  += 1;
        sum        += (s8)*headerByte;
        headerByte += 1;
    }
    *checksum = sum;

    *(useReadBuffer == 0 ? &PARENT_OF(checksum, McSaveState, titleChecksum)->titleChecksumComplement : &PARENT_OF(checksum, McWork, checksum)->checksumComplement) = ~sum;
}

/// Return whether an eligible save matches the read card file header's checksum.
///
/// Borrows a save and dialog work with an already-computed card-header sum.
/// Cheat-mode or demo saves return zero; other saves compare the unsigned
/// 16-bit `titleChecksum` with `work->checksum`. Neither complement nor payload
/// is checked. Retained unused out-of-line entry point.
static s32 _mcMatchesCardHeaderChecksum(const McSaveData* save, const McWork* work)
{
    if (save->state.cheatMode != 0) {
        return 0;
    }
    if (save->state.demoScene != 0) {
        return 0;
    }
    return save->state.titleChecksum == work->checksum;
}

/// Initialize the shared work fields needed when a save dialog starts.
///
/// Selects the first controller port, a No closing answer and the single-line
/// corruption notice, then advances. Entry requires no outstanding transfer:
/// clearing `buffer` does not release an earlier allocation or clear the directory.
static void _mcStateInitSaveWork(Task* task, McWork* work)
{
    enum { MEMORY_CARD_CHANNEL_FIRST_PORT = 0 };

    work->promptTimer        = MEMORY_CARD_PROMPT_LEAD_FRAMES;
    work->cardTimer          = 0;
    work->buffer             = NULL;
    work->channel            = MEMORY_CARD_CHANNEL_FIRST_PORT;
    work->closeAnswer        = USER_INTERFACE_LIST_COMMAND_NO;
    work->corruptNoticeStyle = MEMORY_CARD_CORRUPT_NOTICE_PROMPT;
    task->state++;
}

/// Checksum the live save sections and initialize the complete save transfer walk.
///
/// Selects all nine sections and requires overwrite confirmation. A nonzero
/// first spawn value skips the save question and arms a two-run card retry;
/// zero enters the save question. Does not allocate or submit card I/O.
static void _mcStatePrepareSaveSections(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_SAVE_RETRY_INITIAL_FRAMES = 2,
        MEMORY_CARD_SAVE_STATE_DELAY_RETRY    = 0x27,
    };
    work->slotsRemaining   = MEMORY_CARD_BUFFER_SLOT_COUNT;
    work->slotWriteMask    = MEMORY_CARD_SLOT_WRITE_ALL;
    work->confirmOverwrite = MEMORY_CARD_OVERWRITE_CONFIRM;
    _mcWriteSaveSectionChecksums();

    if (task->spawnArg1.value != 0) {
        task->killCountdown = MEMORY_CARD_SAVE_RETRY_INITIAL_FRAMES;
        task->state         = MEMORY_CARD_SAVE_STATE_DELAY_RETRY;
    } else {
        task->state = MEMORY_CARD_SAVE_STATE_CONFIRM_SAVE;
    }
}

/// Submit card acceptance for the save dialog and draw its checking prompt.
///
/// Acceptance advances to polling; refusal stays here. The card counter becomes
/// one on acceptance and grows by two on refusal. Moves the signed prompt timer
/// toward zero in two-frame steps. The task must borrow a live dialog `UiObject`.
static void _mcStateAcceptSaveCard(Task* task, McWork* work)
{
    enum { MEMORY_CARD_PROMPT_APPROACH_STEP_FRAMES = 2 };

    work->promptId = MEMORY_CARD_PROMPT_CHECKING;
    if (MemCardAccept(work->channel) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    // Retain the extra increment even when the SDK accepts the request.
    work->cardTimer = work->cardTimer + 1;
    _mcDrawPrompt(task, work->promptId);
    // The second test sees the first step: an odd positive timer stays at one.
    if (work->promptTimer > 0) {
        work->promptTimer -= MEMORY_CARD_PROMPT_APPROACH_STEP_FRAMES;
    }
    if (work->promptTimer < 0) {
        work->promptTimer += MEMORY_CARD_PROMPT_APPROACH_STEP_FRAMES;
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

/// Arm the settle delay before opening the selected save file for writing.
///
/// Draws the current prompt and advances to the open state. `promptId` must index
/// `Mc_PromptTable`, and the task must borrow a live dialog `UiObject`.
static void _mcStateBeginOpenSaveFile(Task* task, McWork* work)
{
    work->cardTimer = MEMORY_CARD_IO_SETTLE_FRAMES;
    _mcDrawPrompt(task, work->promptId);
    task->state = task->state + 1;
}

/// Ask whether to create a save file, while detecting card removal or replacement.
///
/// Yes enters file creation; No restores the previous filename and arms a retry.
/// Card polling follows the answer, so a completed error overrides it, closes any
/// choice child and returns to card acceptance. Requires a live dialog object.
static void _mcStateConfirmFileCreation(Task* task, McWork* work)
{
    s32 promptAnswer;
    s32 syncState;

    work->promptId = MEMORY_CARD_PROMPT_CREATE;
    promptAnswer   = _mcUpdateYesNoPrompt(task, MEMORY_CARD_PROMPT_CREATE, work->promptTimer);
    if (promptAnswer != MEMORY_CARD_PROMPT_ANSWER_NO) {
        if (promptAnswer == MEMORY_CARD_PROMPT_ANSWER_YES) {
            task->state = MEMORY_CARD_SAVE_STATE_BEGIN_CREATE;
        }
    } else {
        _mcCopyFileName(true);
        task->killCountdown = MEMORY_CARD_SAVE_RETRY_DELAY_FRAMES;
        task->state         = MEMORY_CARD_SAVE_STATE_DELAY_RETRY;
    }
    syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &work->syncCommand, &work->syncResult);
    if (syncState != MEMORY_CARD_SYNC_IDLE) {
        if (syncState == MEMORY_CARD_SYNC_COMPLETE) {
            if (work->syncResult != McErrNone) {
                task->state = MEMORY_CARD_SAVE_STATE_ACCEPT_CARD;
                _mcCloseChildUi(task, syncState);
            }
        }
    } else {
        MemCardExist(work->channel);
    }
}

/// Show the saving prompt and arm the settle delay before creating a card file.
///
/// Advances to file creation; the task must borrow a live dialog `UiObject`.
static void _mcStateBeginCreateFile(Task* task, McWork* work)
{
    work->cardTimer = MEMORY_CARD_IO_SETTLE_FRAMES;
    work->promptId  = MEMORY_CARD_PROMPT_SAVING;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_SAVING);
    task->state = task->state + 1;
}

/// Enter the card file-header write with the saving prompt and a clear I/O counter.
///
/// Advances to request submission; the task must borrow a live dialog `UiObject`.
static void _mcStateBeginFileHeaderWrite(Task* task, McWork* work)
{
    work->cardTimer = 0;
    task->state++;
    work->promptId = MEMORY_CARD_PROMPT_SAVING;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_SAVING);
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

/// Ask whether to save, then retry card acceptance or close with the retained answer.
///
/// Yes arms the retry delay; No enters save-file closure. Draws through the
/// question helper and steps the signed prompt timer toward zero by two frames.
/// The second timer test observes the first step, so an odd positive count stays at 1.
static void _mcStateConfirmSave(Task* task, McWork* work)
{
    s32 promptAnswer;

    work->promptId = MEMORY_CARD_PROMPT_SAVE;
    promptAnswer   = _mcUpdateYesNoPrompt(task, MEMORY_CARD_PROMPT_SAVE, work->promptTimer);
    switch (promptAnswer) {
        case MEMORY_CARD_PROMPT_ANSWER_PENDING:
            break;
        case MEMORY_CARD_PROMPT_ANSWER_YES:
            task->killCountdown = MEMORY_CARD_SAVE_RETRY_DELAY_FRAMES;
            task->state         = MEMORY_CARD_SAVE_STATE_DELAY_RETRY;
            break;
        case MEMORY_CARD_PROMPT_ANSWER_NO:
            task->state = MEMORY_CARD_SAVE_STATE_CLOSE_PROMPT;
            break;
    }
    if (work->promptTimer > 0) {
        work->promptTimer -= MEMORY_CARD_PROMPT_APPROACH_STEP_FRAMES;
    }
    if (work->promptTimer < 0) {
        work->promptTimer += MEMORY_CARD_PROMPT_APPROACH_STEP_FRAMES;
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

/// Close the save file and publish the dialog's answer before prompt dismissal.
///
/// The task must borrow a live dialog object. Draws the current prompt, enters
/// dismissal and publishes a Cancel UI outcome with the low signed halfword
/// of `closeAnswer`; the outcome requests closure, independently of the answer.
static void _mcStateCloseSaveFile(Task* task, McWork* work)
{
    enum { MEMORY_CARD_SAVE_STATE_DISMISS_PROMPT = 0x1B };
    UiObject* closingObject;
    s16       closeAnswer;

    MemCardClose();
    _mcDrawPrompt(task, work->promptId);
    task->state   = MEMORY_CARD_SAVE_STATE_DISMISS_PROMPT;
    closingObject = task->spawnArg2.pointer;
    if (closingObject != NULL) {
        closeAnswer                = work->closeAnswer;
        closingObject->result      = USER_INTERFACE_RESULT_CANCEL;
        closingObject->resultValue = closeAnswer;
    }
}

/// Kill this dialog task when its callback-owned countdown is nonzero.
///
/// Does not decrement the counter: any positive or negative value requests
/// teardown. Zero keeps the task alive. The state-table work argument is unread.
static void _mcStateKillSaveDialogIfRequested(Task* task, McWork* unusedWork)
{
    if (task->killCountdown != 0) {
        taskKill(task);
    }
}

/// Wait for a card in the save dialog, or close when its Cancel action is confirmed.
///
/// Polls nonblocking card presence, submitting a probe while the SDK is idle.
/// A completed result other than no-card closes the live choice child, restores
/// dialog input and returns to card acceptance, which interprets that card.
/// The task must borrow a live dialog and any first child its live choice object.
static void _mcStateWaitSaveCardInsertion(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_SYNC_IDLE     = -1,
        MEMORY_CARD_SYNC_PENDING  = 0,
        MEMORY_CARD_SYNC_COMPLETE = 1,
        MEMORY_CARD_SYNC_POLL     = 1,
    };
    s32 syncState;

    work->promptId = MEMORY_CARD_PROMPT_NO_CARD;
    if (_mcUpdateCancelPrompt(task, MEMORY_CARD_PROMPT_NO_CARD, work->promptTimer) != 0) {
        task->state = MEMORY_CARD_SAVE_STATE_CLOSE_PROMPT;
        return;
    }
    syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &work->syncCommand, &work->syncResult);
    switch (syncState) {
        case MEMORY_CARD_SYNC_IDLE:
            MemCardExist(work->channel);
            return;
        case MEMORY_CARD_SYNC_COMPLETE:
            if (work->syncResult != McErrCardNotExist) {
                _mcCloseChildUi(task, syncState);
                task->state = MEMORY_CARD_SAVE_STATE_ACCEPT_CARD;
            }
            return;
        case MEMORY_CARD_SYNC_PENDING:
            return;
    }
}

/// Ask to format an unformatted card, initially selecting No, while polling its presence.
///
/// Yes enters the format delay; No arms the delay before returning to the save
/// question. A completed card error overrides either answer, closes the choice
/// child and returns to card acceptance. Requires a live dialog object.
static void _mcStateConfirmCardFormat(Task* task, McWork* work)
{
    s32 promptAnswer;
    s32 syncState;

    work->promptId = MEMORY_CARD_PROMPT_UNFORMATTED;
    promptAnswer   = _mcUpdateYesNoPromptInitialNo(task, MEMORY_CARD_PROMPT_UNFORMATTED, work->promptTimer);
    switch (promptAnswer) {
        case MEMORY_CARD_PROMPT_ANSWER_PENDING:
            break;
        case MEMORY_CARD_PROMPT_ANSWER_YES:
            task->state = MEMORY_CARD_SAVE_STATE_BEGIN_FORMAT;
            break;
        case MEMORY_CARD_PROMPT_ANSWER_NO:
            task->killCountdown = MEMORY_CARD_SAVE_RETRY_DELAY_FRAMES;
            task->state         = MEMORY_CARD_SAVE_STATE_DELAY_CONFIRMATION;
            break;
    }
    syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &work->syncCommand, &work->syncResult);
    if (syncState != MEMORY_CARD_SYNC_IDLE) {
        if (syncState == MEMORY_CARD_SYNC_COMPLETE) {
            if (work->syncResult != McErrNone) {
                task->state = MEMORY_CARD_SAVE_STATE_ACCEPT_CARD;
                _mcCloseChildUi(task, syncState);
            }
        }
    } else {
        MemCardExist(work->channel);
    }
}

/// Show the formatting prompt and arm the delay before the synchronous format call.
///
/// Advances to formatting with a fourteen-frame settle timer. The dialog object
/// must be live; this state submits no card operation itself.
static void _mcStateBeginCardFormat(Task* task, McWork* work)
{
    work->promptId = MEMORY_CARD_PROMPT_FORMATTING;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_FORMATTING);
    work->cardTimer = MEMORY_CARD_IO_SETTLE_FRAMES;
    task->state     = task->state + 1;
}

/// Wait for removal of a full card, or close the save dialog when Cancel is confirmed.
///
/// A completed no-card probe closes any choice child and enters insertion recovery.
/// Other completed results keep waiting; an idle SDK starts another presence probe.
/// Cancel returns before polling. The dialog and any first choice child must be live.
static void _mcStateWaitFullCardRemoval(Task* task, McWork* work)
{
    s32 syncState;
    s32 presenceResult;

    work->promptId = MEMORY_CARD_PROMPT_CARD_FULL;
    if (_mcUpdateCancelPrompt(task, MEMORY_CARD_PROMPT_CARD_FULL, work->promptTimer) != 0) {
        task->state = MEMORY_CARD_SAVE_STATE_CLOSE_PROMPT;
        return;
    }
    syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &work->syncCommand, &work->syncResult);
    switch (syncState) {
        case MEMORY_CARD_SYNC_IDLE:
            MemCardExist(work->channel);
            return;
        case MEMORY_CARD_SYNC_COMPLETE:
            presenceResult = work->syncResult;
            if (presenceResult == McErrCardNotExist) {
                _mcCloseChildUi(task, presenceResult);
                task->state = MEMORY_CARD_SAVE_STATE_NO_CARD;
            }
            return;
        case MEMORY_CARD_SYNC_PENDING:
            return;
    }
}

/// Show an access failure and invalidate both filenames when OK closes the save dialog.
///
/// Clears the I/O wait counter each run. Requires a live dialog and retains both
/// product prefixes and each filename's final three backing bytes.
static void _mcStateAcknowledgeSaveAccessFailure(Task* task, McWork* work)
{
    work->promptId  = MEMORY_CARD_PROMPT_ACCESS_FAILED;
    work->cardTimer = 0;
    if (_mcUpdateOkPrompt(task, work->promptId, 0) != 0) {
        _mcInvalidateFileNameSuffixes();
        task->state = MEMORY_CARD_SAVE_STATE_CLOSE_PROMPT;
    }
}

/// Count down the save dialog's closing prompt, then disable state dispatch.
///
/// Decrements once per run and draws the current prompt. Below the dismiss limit,
/// clears the task countdown and stores the negative state used by the save
/// dispatcher. `promptId` must index `Mc_PromptTable`; the dialog object is live.
static void _mcStateDismissSavePrompt(Task* task, McWork* work)
{
    work->promptTimer -= 1;
    _mcDrawPrompt(task, work->promptId);
    if (work->promptTimer < MEMORY_CARD_PROMPT_DISMISS_LIMIT) {
        task->killCountdown = 0;
        task->state         = MEMORY_CARD_SAVE_STATE_DISMISSED;
    }
}

/// Redraw the prompt and return to the save dialog's directory initialization.
///
/// Requires a live dialog object and a valid `promptId`; the destination arms
/// the directory delay and changes the prompt to card checking.
static void _mcStateRestartSaveDirectory(Task* task, McWork* work)
{
    enum { MEMORY_CARD_SAVE_STATE_BEGIN_DIRECTORY = 0x1F };
    _mcDrawPrompt(task, work->promptId);
    task->state = MEMORY_CARD_SAVE_STATE_BEGIN_DIRECTORY;
}

/// Hold the saving prompt until the card delay expires, then close with Yes.
///
/// Tests `cardTimer` before decrementing it: zero or less enters the save closing
/// state on this run. The task must borrow a live dialog `UiObject`.
static void _mcStateWaitSaveClose(Task* task, McWork* work)
{
    work->promptId = MEMORY_CARD_PROMPT_SAVING;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_SAVING);
    if (work->cardTimer-- <= 0) {
        work->closeAnswer = USER_INTERFACE_LIST_COMMAND_YES;
        task->state       = MEMORY_CARD_SAVE_STATE_CLOSE_PROMPT;
    }
}

/// Show the checking prompt and arm the save dialog's directory-scan delay.
///
/// Advances to the block-owner scan; the task must borrow a live dialog `UiObject`.
static void _mcStateBeginSaveDirectory(Task* task, McWork* work)
{
    work->cardTimer = MEMORY_CARD_IO_BRIEF_FRAMES;
    work->promptId  = MEMORY_CARD_PROMPT_CHECKING;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_CHECKING);
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

/// Submit the current directory file's 128-byte preview read for the save list.
///
/// The file is already open and `currentSlot` must be in 0..`entryCount` - 1
/// (0..14). Reads byte offset 0x200 into the work's word-aligned
/// preview through the SDK word-pointer transport view. Acceptance clears the
/// I/O counter and enters polling; refusal counts a frame and retries. The work
/// must remain live until completion; the current prompt is drawn each run.
static void _mcStateReadSavePreview(Task* task, McWork* work)
{

    if (MemCardReadData((u_long*)&work->previews[work->currentSlot], MEMORY_CARD_SAVE_PREVIEW_FILE_OFFSET,
                        sizeof(work->previews[work->currentSlot])) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    _mcDrawPrompt(task, work->promptId);
}

/// Finish a save-list preview read and advance to the next directory entry.
///
/// After a successful polled read, closes the file and increments `currentSlot`.
/// Opens another preview while below `entryCount`, otherwise enters file
/// selection. Read errors show access failure. At most fifteen previews borrow
/// work storage; the current prompt is drawn on every run.
static void _mcStateAdvanceSavePreview(Task* task, McWork* work)
{
    enum { MEMORY_CARD_SAVE_STATE_OPEN_PREVIEW = 0x22 };
    s32 nextDirectoryIndex;

    if (work->syncResult == McErrNone) {
        MemCardClose();
        nextDirectoryIndex = work->currentSlot + 1;
        work->currentSlot  = nextDirectoryIndex;
        if (nextDirectoryIndex < work->entryCount) {
            task->state = MEMORY_CARD_SAVE_STATE_OPEN_PREVIEW;
        } else {
            task->state = task->state + 1;
        }
    } else {
        task->state = MEMORY_CARD_SAVE_STATE_ACCESS_FAILED;
    }
    _mcDrawPrompt(task, work->promptId);
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

/// Draw the saving prompt during the delay before preparing a section write.
///
/// The overwrite confirmation arms `cardTimer` in frames. Decrements it once
/// per run and enters section preparation when the new value is zero or below.
/// The task must borrow a live dialog `UiObject`; no card request is submitted.
static void _mcStateDelaySectionWrite(Task* task, McWork* work)
{
    work->cardTimer -= 1;
    if (work->cardTimer <= 0) {
        task->state = MEMORY_CARD_SAVE_STATE_PREPARE_SECTION;
    }
    work->promptId = MEMORY_CARD_PROMPT_SAVING;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_SAVING);
}

/// Show save failure until OK enters save-file closure.
///
/// Resets the I/O wait counter each run; the dialog object must be live.
static void _mcStateAcknowledgeSaveFailure(Task* task, McWork* work)
{
    work->promptId  = MEMORY_CARD_PROMPT_SAVE_FAILED;
    work->cardTimer = 0;
    if (_mcUpdateOkPrompt(task, MEMORY_CARD_PROMPT_SAVE_FAILED, 0) != 0) {
        task->state = MEMORY_CARD_SAVE_STATE_CLOSE_PROMPT;
    }
}

/// Show format failure until OK enters save-file closure.
///
/// Resets the I/O wait counter each run; the dialog object must be live.
static void _mcStateAcknowledgeFormatFailure(Task* task, McWork* work)
{
    work->promptId  = MEMORY_CARD_PROMPT_FORMAT_FAILED;
    work->cardTimer = 0;
    if (_mcUpdateOkPrompt(task, MEMORY_CARD_PROMPT_FORMAT_FAILED, 0) != 0) {
        task->state = MEMORY_CARD_SAVE_STATE_CLOSE_PROMPT;
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
        _mcStateKillSaveDialogIfRequested(task, work);
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

/// Initialize the shared work for loading and clear a pending player-position update.
///
/// Selects the first card port, the load question and multiline corruption notice,
/// then advances to section selection. Entry requires no outstanding allocation:
/// clearing `buffer` does not free it. The cached directory and filenames are retained.
static void _mcStateInitLoadWork(Task* task, McWork* work)
{
    enum { MEMORY_CARD_CHANNEL_FIRST_PORT = 0 };
    work->promptTimer                            = MEMORY_CARD_PROMPT_LEAD_FRAMES;
    work->promptId                               = MEMORY_CARD_PROMPT_LOAD;
    work->corruptNoticeStyle                     = MEMORY_CARD_CORRUPT_NOTICE_MULTILINE;
    work->cardTimer                              = 0;
    work->buffer                                 = NULL;
    work->channel                                = MEMORY_CARD_CHANNEL_FIRST_PORT;
    gDisplayState.control.flags.pendingPlayerPos = 0;
    task->state                                 += 1;
}

/// Select all nine file sections for loading, then enter card acceptance.
///
/// Initializes the remaining-section count and shifted transfer mask, including
/// section zero's card file header. Used after load-dialog initialization;
/// the later directory and file-open states choose the file and transfer offset.
static void _mcStateInitLoadSections(Task* task, McWork* work)
{
    enum { MEMORY_CARD_LOAD_STATE_ACCEPT_CARD = 7 };

    work->slotsRemaining = MEMORY_CARD_BUFFER_SLOT_COUNT;
    work->slotWriteMask  = MEMORY_CARD_SLOT_WRITE_ALL;
    task->state          = MEMORY_CARD_LOAD_STATE_ACCEPT_CARD;
}

/// Show the load question once its prompt lead-in reaches exactly zero.
///
/// Positive frame counts step down by two. An even nonnegative count reaches
/// zero; odd positive or negative counts keep drawing the current prompt without
/// opening choices. Yes returns to card acceptance; No closes the load file.
/// The task must borrow a live dialog and any first child its choice object.
static void _mcStateConfirmLoadAfterDelay(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_PROMPT_LEAD_STEP_FRAMES = 2,
        MEMORY_CARD_PROMPT_ANSWER_YES       = 1,
        MEMORY_CARD_PROMPT_ANSWER_NO        = -1,
        MEMORY_CARD_LOAD_STATE_ACCEPT_CARD  = 7,
        MEMORY_CARD_LOAD_STATE_CLOSE_FILE   = 3,
    };
    s32 promptAnswer;

    if (work->promptTimer > 0) {
        work->promptTimer -= MEMORY_CARD_PROMPT_LEAD_STEP_FRAMES;
    }
    if (work->promptTimer == 0) {
        work->promptId = MEMORY_CARD_PROMPT_LOAD;
        promptAnswer   = _mcUpdateYesNoPrompt(task, MEMORY_CARD_PROMPT_LOAD, work->promptTimer);
        switch (promptAnswer) {
            case MEMORY_CARD_PROMPT_ANSWER_PENDING:
                break;
            case MEMORY_CARD_PROMPT_ANSWER_YES:
                task->state = MEMORY_CARD_LOAD_STATE_ACCEPT_CARD;
                break;
            case MEMORY_CARD_PROMPT_ANSWER_NO:
                task->state = MEMORY_CARD_LOAD_STATE_CLOSE_FILE;
                break;
        }
    } else {
        _mcDrawPrompt(task, work->promptId);
    }
}

/// Close the load file and request UI cancellation before prompt dismissal.
///
/// The task must borrow a live dialog object. Draws the current prompt and
/// enters dismissal. Publishes the Cancel UI outcome without changing the
/// dialog's retained result value.
static void _mcStateCloseLoadFile(Task* task, McWork* work)
{
    enum { MEMORY_CARD_LOAD_STATE_DISMISS_PROMPT = 4 };
    UiObject* closingObject;

    MemCardClose();
    _mcDrawPrompt(task, work->promptId);
    task->state   = MEMORY_CARD_LOAD_STATE_DISMISS_PROMPT;
    closingObject = task->spawnArg2.pointer;
    if (closingObject != NULL) {
        closingObject->result = USER_INTERFACE_RESULT_CANCEL;
    }
}

/// Count down the file-select dialog's closing prompt, then enter its kill state.
///
/// Decrements once per run and draws the current prompt. Below the dismiss limit,
/// clears the task countdown and advances. `promptId` must index `Mc_PromptTable`,
/// and the task must borrow a live dialog `UiObject`.
static void _mcStateDismissLoadPrompt(Task* task, McWork* work)
{
    work->promptTimer -= 1;
    _mcDrawPrompt(task, work->promptId);
    if (work->promptTimer < MEMORY_CARD_PROMPT_DISMISS_LIMIT) {
        task->killCountdown = 0;
        task->state         = task->state + 1;
    }
}

/// Kill this dialog task when its callback-owned countdown is nonzero.
///
/// Does not decrement the counter: any positive or negative value requests
/// teardown. Zero keeps the task alive. The state-table work argument is unread.
static void _mcStateKillLoadDialogIfRequested(Task* task, McWork* unusedWork)
{
    if (task->killCountdown != 0) {
        taskKill(task);
    }
}

/// Show load failure and invalidate both filenames when OK enters load-file closure.
///
/// Clears the I/O wait counter each run. Requires a live dialog; filename product
/// prefixes and final backing bytes are retained. Transfer cleanup is the caller's.
static void _mcStateAcknowledgeLoadFailure(Task* task, McWork* work)
{
    work->promptId  = MEMORY_CARD_PROMPT_LOAD_FAILED;
    work->cardTimer = 0;
    if (_mcUpdateOkPrompt(task, MEMORY_CARD_PROMPT_LOAD_FAILED, 0) != 0) {
        _mcInvalidateFileNameSuffixes();
        task->state = MEMORY_CARD_LOAD_STATE_CLOSE_FILE;
    }
}

/// Submit card acceptance for file selection while drawing the checking prompt.
///
/// Acceptance clears the I/O counter and advances to polling; refusal increments
/// the counter and retries next run. Acceptance does not mean completion.
/// The task must borrow a live dialog `UiObject`.
static void _mcStateAcceptLoadCard(Task* task, McWork* work)
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

/// Wait for a card in the load dialog, or close when its Cancel action is confirmed.
///
/// Polls nonblocking card presence and starts a probe when the SDK is idle.
/// A completed result other than no-card closes the live choice child, restores
/// dialog input and retries card acceptance. The task must borrow a live dialog
/// and any first child its choice object; pending or no-card results keep waiting.
static void _mcStateWaitLoadCardInsertion(Task* task, McWork* work)
{
    enum {
        MEMORY_CARD_SYNC_IDLE              = -1,
        MEMORY_CARD_SYNC_PENDING           = 0,
        MEMORY_CARD_SYNC_COMPLETE          = 1,
        MEMORY_CARD_SYNC_POLL              = 1,
        MEMORY_CARD_LOAD_STATE_ACCEPT_CARD = 7,
        MEMORY_CARD_LOAD_STATE_CLOSE_FILE  = 3,
    };
    s32 syncState;

    work->promptId = MEMORY_CARD_PROMPT_NO_CARD;
    if (_mcUpdateCancelPrompt(task, MEMORY_CARD_PROMPT_NO_CARD, work->promptTimer) != 0) {
        task->state = MEMORY_CARD_LOAD_STATE_CLOSE_FILE;
        return;
    }
    syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &work->syncCommand, &work->syncResult);
    switch (syncState) {
        case MEMORY_CARD_SYNC_IDLE:
            MemCardExist(work->channel);
            return;
        case MEMORY_CARD_SYNC_COMPLETE:
            if (work->syncResult != McErrCardNotExist) {
                _mcCloseChildUi(task, syncState);
                task->state = MEMORY_CARD_LOAD_STATE_ACCEPT_CARD;
            }
            return;
        case MEMORY_CARD_SYNC_PENDING:
            return;
    }
}

/// Wait for removal of a card with no loadable game data, or Cancel the load dialog.
///
/// Polls nonblocking card presence, submitting a probe while the SDK is idle.
/// Only a completed no-card result closes the Cancel child, restores parent input
/// and enters insertion recovery. Other completed results keep the no-data notice.
/// The task and any first child must borrow live dialog/choice objects.
static void _mcStateWaitNoDataCardRemoval(Task* dialogTask, McWork* dialogWork)
{
    enum { MEMORY_CARD_LOAD_STATE_NO_CARD = 0xA };
    s32 syncState;
    s32 presenceResult;

    dialogWork->promptId = MEMORY_CARD_PROMPT_NO_DATA;
    if (_mcUpdateCancelPrompt(dialogTask, MEMORY_CARD_PROMPT_NO_DATA, dialogWork->promptTimer) != 0) {
        dialogTask->state = MEMORY_CARD_LOAD_STATE_CLOSE_FILE;
        return;
    }
    syncState = MemCardSync(MEMORY_CARD_SYNC_POLL, &dialogWork->syncCommand, &dialogWork->syncResult);
    switch (syncState) {
        case MEMORY_CARD_SYNC_IDLE:
            MemCardExist(dialogWork->channel);
            return;
        case MEMORY_CARD_SYNC_COMPLETE:
            presenceResult = dialogWork->syncResult;
            if (presenceResult == McErrCardNotExist) {
                _mcCloseChildUi(dialogTask, presenceResult);
                dialogTask->state = MEMORY_CARD_LOAD_STATE_NO_CARD;
            }
            return;
        case MEMORY_CARD_SYNC_PENDING:
            return;
    }
}

/// Arm the brief delay before opening the selected file for the load walk.
///
/// Draws the current prompt and advances to the open state. `promptId` must index
/// `Mc_PromptTable`, and the task must borrow a live dialog `UiObject`.
static void _mcStateBeginSectionLoad(Task* task, McWork* work)
{
    work->cardTimer = MEMORY_CARD_IO_BRIEF_FRAMES;
    _mcDrawPrompt(task, work->promptId);
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

/// Show the checking prompt and arm the file-select directory-read delay.
///
/// Advances to directory enumeration; the task must borrow a live dialog `UiObject`.
static void _mcStateBeginLoadDirectory(Task* task, McWork* work)
{
    work->cardTimer = MEMORY_CARD_IO_BRIEF_FRAMES;
    work->promptId  = MEMORY_CARD_PROMPT_CHECKING;
    _mcDrawPrompt(task, MEMORY_CARD_PROMPT_CHECKING);
    task->state = task->state + 1;
}

/// Read this product's card directory when the file-select delay reaches zero.
///
/// Entry requires a positive `cardTimer`, a valid prompt row and a live dialog
/// `UiObject`. The SDK may fill up to `MEMORY_CARD_DIRECTORY_CAPACITY` entries.
/// A nonempty list starts preview reads at index zero; an empty list shows the
/// no-data prompt. The SDK return is ignored and the current prompt is always drawn.
static void _mcStateReadLoadDirectory(Task* task, McWork* work)
{
    work->cardTimer -= 1;
    if (work->cardTimer == 0) {
        work->entryCount = 0;
        MemCardGetDirentry(
            work->channel, (char*)Mc_SaveFilePattern, work->directory, &work->entryCount, 0,
            ARRAY_SIZE(work->directory));
        if (work->entryCount != 0) {
            work->selectedSlot = 0;
            work->currentSlot  = 0;
            task->state        = task->state + 1;
        } else {
            task->state = MEMORY_CARD_LOAD_STATE_NO_DATA;
        }
    }

    _mcDrawPrompt(task, work->promptId);
}

/// Open the current directory entry for its load-list preview.
///
/// `currentSlot` must index the populated directory (0..`entryCount` - 1,
/// at most fifteen entries). Closes the preceding file, advances to preview
/// reading on success or shows load failure on error, and draws the prompt.
static void _mcStateOpenLoadPreview(Task* task, McWork* work)
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
        task->state = MEMORY_CARD_LOAD_STATE_FAILED;
    }
    _mcDrawPrompt(task, work->promptId);
}

/// Submit the current directory file's 128-byte preview read for the load list.
///
/// The current file is open and `currentSlot` indexes the populated directory
/// (0..`entryCount` - 1, bounded by 0..14). The SDK word pointer transports the
/// word-aligned preview's complete bytes at file offset 0x200. Acceptance enters
/// polling and clears the counter; refusal counts a frame and retries. Work
/// storage must survive completion, and the live dialog's prompt is drawn.
static void _mcStateReadLoadPreview(Task* task, McWork* work)
{

    if (MemCardReadData((u_long*)&work->previews[work->currentSlot], MEMORY_CARD_SAVE_PREVIEW_FILE_OFFSET,
                        sizeof(work->previews[work->currentSlot])) != 0) {
        work->cardTimer = 0;
        task->state     = task->state + 1;
    } else {
        work->cardTimer = work->cardTimer + 1;
    }
    _mcDrawPrompt(task, work->promptId);
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

/// Show unusable save data and invalidate both filenames when OK closes the load file.
///
/// Clears the I/O wait counter each run. Requires a live dialog; filename product
/// prefixes and final backing bytes are retained. Transfer cleanup is the caller's.
static void _mcStateAcknowledgeCorruptLoad(Task* task, McWork* work)
{
    work->promptId  = MEMORY_CARD_PROMPT_CORRUPTED;
    work->cardTimer = 0;
    if (_mcUpdateOkPrompt(task, MEMORY_CARD_PROMPT_CORRUPTED, 0) != 0) {
        _mcInvalidateFileNameSuffixes();
        task->state = MEMORY_CARD_LOAD_STATE_CLOSE_FILE;
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
