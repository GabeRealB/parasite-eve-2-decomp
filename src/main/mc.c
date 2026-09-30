#include "mc.h"

#include <psyq/sys/types.h>
#include <psyq/kernel.h>
#include <psyq/libmcrd.h>
#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/strings.h>

#include "common.h"

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

/// Serialized preview range and place-label values used by the slot UI.
enum {
    MEMORY_CARD_SAVE_PREVIEW_FILE_OFFSET   = 0x200,
    MEMORY_CARD_SAVE_HEADER_CHECKSUM_BYTES = 0x38,
    MEMORY_CARD_SAVE_POINT_COUNT           = 16,
    MEMORY_CARD_SAVE_POINT_OPENING         = 15,
};

/// Pair of prompt / dialog data pointers. Mc_PromptTable is an array of these,
/// indexed by mode (see Mc_DrawPrompt).
typedef struct _McPromptPair {
    /* 0x0 */ u8* field_0;
    /* 0x4 */ u8* field_4;
} McPromptPair;
STATIC_ASSERT_SIZEOF(McPromptPair, 0x8);

/// Prompt helpers first locate a child task, then replace this handle with
/// its UI object (or a newly spawned object). Each phase reads the member it
/// assigned; the task and UI object are never reinterpreted as each other.
typedef union {
    Task*     task;
    UiObject* object;
} McPromptChild;
STATIC_ASSERT_SIZEOF(McPromptChild, 4);

/// Checksummed buffer header (sum / ones-complement at 0x0 / 0x2, payload at 0x4).
typedef struct _McChecksumBlock {
    /* 0x0 */ s16 checksum;
    /* 0x2 */ s16 checksumComplement;
    /* 0x4 */ u8  payload[0]; // Variable payload; length comes from McBufferSlot.
} McChecksumBlock;

/// 0xC descriptor for a memcard/save buffer slot in Mc_BufferSlots[9].
/// buffer holds two checksummed copies, each bytesPerCopy bytes long.
/// Iterated from index 1..8 by Mc_VerifyFirstByteChecksum and related helpers in mc.c.
typedef struct _McBufferSlot {
    /* 0x0 */ McChecksumBlock* buffer;
    /* 0x4 */ s32              bytesPerCopy;
    /* 0x8 */ s32              cardSectors;
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

/* Define BSS before API headers to preserve first-declaration order. */
/// Work area of the memory-card dialogs, passed to every state handler.
static McWork Mc_MenuWork;

McSaveData gMcSaveData[MEMORY_CARD_SAVE_COUNT];

GameFlagAcropolisBank GameFlag_AcropolisBanks[2];

GameFlagDryfieldBank GameFlag_DryfieldBanks[2];

GameFlagDryfieldFullBank GameFlag_DryfieldFullBanks[2];

GameFlagShelterBank GameFlag_ShelterBanks[2];

GameFlagNeoArkBank GameFlag_NeoArkBanks[2];

GameFlagNibbleBank gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_COUNT];

/// Unreferenced.
static u8 D_80073B80[8];

PlayerStatus Player_Status;

/// Last `rand()` result drawn by `Mc_DispatchStateTable`; nothing reads it.
static s32 Mc_LastRandomValue;

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/wipsys.h"

/// Number of 128-byte card sectors holding the complete game-flag bank pair.
enum { GAME_FLAG_NIBBLE_BANK_CARD_SECTORS = 4 };

extern McBufferSlot Mc_BufferSlots[9];

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

static UiListItemFunc Mc_SaveSlotCallbacks[];

static UiListItemFunc Mc_LoadSlotCallbacks[];

static u8* Mc_ModeLabels[];

static UiObjectDesc Mc_SaveListDesc[];

static UiObjectDesc Mc_LoadListDescriptors[];

static const char McText_CloseParen[];

static const McStateFuncTable44 Mc_PromptStates;

/// Jump table of 26 McStateFunc handlers used by Mc_DispatchStateTable26.
static const McStateFuncTable26 Mc_FileSelectStates;

static void Mc_BuildFileName(u8* arg0, s32 arg1);

static void Mc_InitDualBankBuffers(void);

/// Store the checksum of a buffer's payload (the `size - 4` bytes after its
/// header) in the header: the signed byte sum and its complement.
static inline void _mcWriteBlockChecksum(u8* data, s32 size);

/// Prompt + optional choice dialog (Mc_PromptTable[mode]).
static s32 Mc_PromptDialog(Task* task, s32 arg1, s32 unused3);

static s32 Mc_PromptDialogChoice(Task* task, s32 arg1, s32 unused3);

static s32 Mc_PromptDialogSpawn(Task* task, s32 arg1, s32 unused3);

static s32 Mc_PromptDialogFile(Task* task, s32 arg1, s32 unused3);

static inline u16* Mc_EncodeTitleText(s8* arg0, u16* arg1);

static inline u16* Mc_EncodeTitleLiteral(s8* arg0, u16* arg1);

static inline void Mc_UpdateTitleHeaderChecksum(void);

static inline u8* Mc_CopyTitleBytes(u8* src, u8* dst);

static inline void Mc_UpdateTitleDataChecksum(void);

/// Chooses the save number and builds the card title and its checksums.
static void Mc_BuildSaveTitle(McWork* work);

static void Mc_StateScanDirFlags(Task* task, McWork* work);

static void Mc_StateListDirectory(Task* task, McWork* work);

/// Inline form of Mc_DrawPrompt.
static inline void _mcDrawPrompt(Task* task, s32 mode);

/// Tear down the task's child UI and report status on the task's own object.
static inline void _mcCloseChild(Task* task, s32 status);

/// Inline form of Mc_CopyFileName: 0 saves Mc_FileName to Mc_FileNameBuf, otherwise restores it.
static inline void _mcCopyFileName(s32 arg0);

static void Mc_StateFileSelect(Task* task, McWork* work);

/// Mask of the buffer slots to write, bit n for slot n: set where the slot's
/// two halves differ, and always for slots 0, 1 and 8.
static inline s32 _mcCompareBufferHalves(void);

static void Mc_StateCompareBuffers(Task* task, McWork* work);

static void Mc_StateOpenRead(Task* task, McWork* work);

static void Mc_StateCreateFile(Task* task, McWork* work);

static void Mc_StatePadFileName(Task* task, McWork* work);

static void Mc_StateNameEntry(Task* task, McWork* work);

/// Copy the first half of each of Mc_BufferSlots[1..8] over its second half.
static inline void _mcCopyBufferHalves(void);

/// Inline form of Mc_WriteFirstByteChecksum.
static inline void _mcWriteFirstByteChecksum(void);

static void Mc_StateBackupBuffers(Task* task, McWork* work);

static void Mc_StateFreeBuffer(Task* task, McWork* work);

static void Mc_StateFormat(Task* task, McWork* work);

static void Mc_StateSyncFileSelect(Task* task, McWork* work);

static void Mc_StateBlankFileName(Task* task, McWork* work);

static void Mc_StateSyncOpen(Task* task, McWork* work);

/// Inline form of Mc_VerifySlotChecksums.
static inline s32 _mcVerifySlotChecksums(void);

/// Inline form of Mc_WriteSlotChecksums: store the signed byte sum of each
/// buffer slot 1..8's payload, and its complement, in the buffer's header.
static inline void _mcWriteSlotChecksums(void);

/// Inline form of Mc_VerifyFirstByteChecksum.
static inline s32 _mcVerifyFirstByteChecksum(void);

static void Mc_StateVerifyFinish(Task* task, McWork* work);

/// Checksum the 0x200-byte work buffer into the work's sum / complement pair,
/// clearing the pair first.
static inline void _mcWriteWorkChecksum(McWork* work);

static void Mc_StateFinishWrite(Task* task, McWork* work);

/// Inline form of Mc_VerifySaveHdrChecksum: whether a save header names a valid
/// save point and carries the checksum of its 56 bytes from `McSavePreview.location`.
static inline s32 _mcVerifySaveHdrChecksum(McSavePreview* save);

static void Mc_StateSaveSlotUi(UiList* list, UiObject* object);

static u16* Mc_EncodeAsciiGlyphs(s8* arg0, u16* arg1);

static void Mc_InitFileName(void);

static void Mc_CopyFileName(s32 arg0);

static void Mc_WriteSaveHdrChecksum(void);

static s32 Mc_VerifySaveHdrChecksum(McSaveData* save);

/// Out-of-line form of `_mcWriteBlockChecksum`. Nothing calls it.
static void Mc_WriteBlockChecksum(u8* data, s32 size);

static void Mc_ClearWorkBuffers(void);

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

static void Mc_StateSyncAdvance(Task* task, McWork* work);

static void Mc_StateDrawPromptAdvance(Task* task, McWork* work);

static void Mc_StatePromptChoiceB(Task* task, McWork* work);

static void Mc_StateDrawPrompt4(Task* task, McWork* work);

static void Mc_StateEnterDialog4(Task* task, McWork* work);

static void Mc_StateWriteFile(Task* task, McWork* work);

static void Mc_StatePromptChoiceGeneric(Task* task, McWork* work);

static void Mc_StateWriteData(Task* task, McWork* work);

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

static void Mc_StateOpenSelected(Task* task, McWork* work);

static void Mc_StateReadHeader(Task* task, McWork* work);

static void Mc_StateOpenNext(Task* task, McWork* work);

static void Mc_StateUiCountdown2(Task* task, McWork* work);

static void Mc_StateUiCountdownE(Task* task, McWork* work);

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

static void Mc_StateReadData(Task* task, McWork* work);

static void Mc_StateDrawPrompt1(Task* task, McWork* work);

static void Mc_StateGetDirentry(Task* task, McWork* work);

static void Mc_StateOpenDirEntry(Task* task, McWork* work);

static void Mc_StateReadSlot(Task* task, McWork* work);

static void Mc_StateWalkDirectory(Task* task, McWork* work);

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

McBufferSlot Mc_BufferSlots[9] = {
    { (McChecksumBlock*)Mc_DefaultChecksumSrc, 0x100, 4 },
    { (McChecksumBlock*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE], sizeof(McSaveData), MEMORY_CARD_SAVE_CARD_SECTORS },
    { (McChecksumBlock*)&Player_Status, PLAYER_STATUS_SAVE_RECORD_BYTES, 1 },
    { (McChecksumBlock*)GameFlag_AcropolisBanks, 0x6C, 2 },
    { (McChecksumBlock*)GameFlag_DryfieldBanks, 0xB0, 3 },
    { (McChecksumBlock*)GameFlag_DryfieldFullBanks, 0x24, 1 },
    { (McChecksumBlock*)GameFlag_ShelterBanks, 0xE4, 4 },
    { (McChecksumBlock*)GameFlag_NeoArkBanks, 0xA4, 3 },
    { (McChecksumBlock*)&gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE], sizeof(gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE]), GAME_FLAG_NIBBLE_BANK_CARD_SECTORS },
};

static UiListItemFunc Mc_SaveSlotCallbacks[] = { Mc_StateSaveSlotUi };
UiList                Mc_SaveSlotList        = { Mc_SaveSlotCallbacks, 0x0F, 0x0F, 0, 0x2E };
static UiListItemFunc Mc_LoadSlotCallbacks[] = { McMenu_ConfirmWithRender };
UiList                Mc_LoadSlotList        = { Mc_LoadSlotCallbacks, 0x0F, 0x0F, 0, 0x2E };

static u8* Mc_ModeLabels[] = { (u8*)McText_Replay, (u8*)McText_Bounty, (u8*)McText_Scavenger, (u8*)McText_Nightmare };

UiObjectDesc Mc_TaskDescriptors[] = {
    { 2, 0xFF70, 0xFFE7, 0x120, 0x32, 0x14, 0, 0, 0xC0, Mc_DispatchStateTable26, 0 },
    { 2, 0xFF70, 0xFFE7, 0x120, 0x32, 0x14, 0, 0, 0xC0, Mc_DispatchStateTable, 0 },
};
static UiObjectDesc Mc_SaveListDesc[] = {
    { 0x80002, 0xFF78, 0x0A, 0x120, 0x3C, 0x0C, 0, 0, 0xC0, McMenu_SelectList, 0 },
};
static UiObjectDesc Mc_LoadListDescriptors[] = {
    { 0x80002, 0xFF78, 0x0A, 0x120, 0x3C, 0x0C, 0, 0, 0xC0, McMenu_SelectListAlt, 0 },
    { 2, 0xFFC4, 0x1E, 0xC8, 0x3C, 0x1C, 0, 0, 0xC0, McMenu_FileInformation, 0 },
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

static void Mc_InitDualBankBuffers(void)
{
    GameFlagAcropolisBank*    a;
    GameFlagDryfieldBank*     b;
    GameFlagDryfieldFullBank* c;
    GameFlagShelterBank*      d;
    GameFlagNeoArkBank*       e;
    McSaveData*               p;
    s32                       one;
    s32                       two;
    s32                       idx;

    Mem_Set(&Player_Status, 0, PLAYER_STATUS_SAVE_RECORD_BYTES);
    Mem_Set(Player_Status.saveBackup, 0xFF, sizeof(Player_Status.saveBackup));
    Mem_Set(&gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE], 0, sizeof(gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE]));
    Mem_Set(&gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_BACKUP], 0xFF, sizeof(gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_BACKUP]));

    a = GameFlag_AcropolisBanks;
    Mem_Set(a, 0, 0x6C);
    do {
        b = GameFlag_DryfieldBanks;
        Mem_Set(b, 0, 0xB0);
        c = GameFlag_DryfieldFullBanks;
        Mem_Set(c, 0, 0x24);
        d = GameFlag_ShelterBanks;
        Mem_Set(d, 0, 0xE4);
        e = GameFlag_NeoArkBanks;
        Mem_Set(e, 0, 0xA4);
        Mem_Set(a + 1, 0xFF, 0x6C);
        Mem_Set(b + 1, 0xFF, 0xB0);
        Mem_Set(c + 1, 0xFF, 0x24);
        Mem_Set(d + 1, 0xFF, 0xE4);
        Mem_Set(e + 1, 0xFF, 0xA4);
        p = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    } while (0);

    one                           = 1;
    p->state.location.loc.area    = 0x14;
    two                           = 2;
    p->state.location.loc.stage   = one;
    p->state.location.loc.view    = one;
    p->state.location.loc.room    = one;
    p->state.location.loc.warp    = 7;
    p->state.location.loc.variant = one;
    p->state.sceneEvent           = two;
    p->state.characterId          = one;
    Player_InitNewGameStats();
    idx                          = p->state.characterId - 1;
    (&Player_Status)[idx].weapon = two;
}

/// Store the checksum of a buffer's payload (the `size - 4` bytes after its
/// header) in the header: the signed byte sum and its complement.
static inline void _mcWriteBlockChecksum(u8* data, s32 size)
{
    McChecksumBlock* block;
    s16              sum;
    u32              i;

    block = (McChecksumBlock*)data;
    sum   = 0;
    data  = block->payload;
    size -= 4;
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

void Mc_InitBufferSlots(void)
{
    McBufferSlot* base;
    McBufferSlot* slot;
    u8*           ptr;
    u32           size;
    u32           i;
    s32           fill;

    fill = -1;
    base = Mc_BufferSlots;
    slot = base + 1;
    do {
        size = slot->bytesPerCopy;
        ptr  = (u8*)slot->buffer;
        for (i = 0; i < size; i++) {
            *ptr++ = 0;
        }
        for (i = 0; i < size; i++) {
            *ptr++ = fill;
        }
        _mcWriteBlockChecksum((u8*)slot->buffer, size);
        slot++;
    } while (slot < base + 9);

    gDisplayState.spriteVariant = 1;
    Mc_InitDualBankBuffers();

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.vibration    = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume  = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cursorMode   = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode    = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode     = 0;
    CdVol_SetMixMode(1);
    Snd_ApplyVolumeTable(0);
}

/// Prompt + optional choice dialog (Mc_PromptTable[mode]).
static s32 Mc_PromptDialog(Task* task, s32 arg1, s32 unused3)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    McPromptChild child;
    McPromptPair* entry;
    McPromptPair* base;

    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, one, 0);

    child.task = task->firstChild;
    if (child.task == NULL) {
        child.object = Ui_SpawnFromDesc(Mc_PromptDesc, one, one, 2, obj);
        if (child.object != NULL) {
            child.object->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 5) - child.object->panel.bounds.unsignedRect.w;
            child.object->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue + 8;
            obj->resultValue                          = 0;
            obj->panel.control.word                   = USER_INTERFACE_PANEL_INACTIVE;
        }
        return 0;
    }
    child.object = child.task->spawnArg2.pointer;
    if (child.object->result == USER_INTERFACE_RESULT_CONFIRM) {
        obj->resultValue = child.object->resultValue;
        Ui_TeardownTree(child.object, child.object->owner);
        obj->panel.control.word = one;
    }
    return obj->resultValue;
}

static s32 Mc_PromptDialogChoice(Task* task, s32 arg1, s32 unused3)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    McPromptChild child;
    McPromptPair* entry;
    McPromptPair* base;

    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, one, 0);

    child.task = task->firstChild;
    if (child.task == NULL) {
        child.object = Ui_SpawnFromDesc(Mc_PromptDesc, 0, one, 2, obj);
        if (child.object != NULL) {
            child.object->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 5) - child.object->panel.bounds.unsignedRect.w;
            child.object->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue + 0x10;
            obj->resultValue                          = 0;
            obj->panel.control.word                   = USER_INTERFACE_PANEL_INACTIVE;
        }
        return 0;
    }
    child.object = child.task->spawnArg2.pointer;
    if (child.object->result == USER_INTERFACE_RESULT_CONFIRM) {
        obj->resultValue = child.object->resultValue;
        Ui_TeardownTree(child.object, child.object->owner);
        obj->panel.control.word = one;
    }
    return obj->resultValue;
}

static s32 Mc_PromptDialogSpawn(Task* task, s32 arg1, s32 unused3)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    McPromptChild child;
    McPromptPair* entry;
    McPromptPair* base;

    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, one, 0);

    child.task = task->firstChild;
    if (child.task == NULL) {
        child.object = Ui_SpawnFromDesc(Mc_PromptDesc, 3, one, 2, obj);
        if (child.object != NULL) {
            child.object->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 5) - child.object->panel.bounds.unsignedRect.w;
            child.object->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue + 0x10;
            obj->resultValue                          = 0;
            obj->panel.control.word                   = USER_INTERFACE_PANEL_INACTIVE;
        }
        return 0;
    }
    child.object = child.task->spawnArg2.pointer;
    if (child.object->result == USER_INTERFACE_RESULT_CONFIRM) {
        obj->resultValue = child.object->resultValue;
        Ui_TeardownTree(child.object, child.object->owner);
        obj->panel.control.word = one;
    }
    return obj->resultValue;
}

static s32 Mc_PromptDialogFile(Task* task, s32 arg1, s32 unused3)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    McPromptChild child;
    McPromptPair* entry;
    McPromptPair* base;

    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, one, 0);

    child.task = task->firstChild;
    if (child.task == NULL) {
        child.object = Ui_SpawnFromDesc(Mc_PromptDesc, 2, one, 2, obj);
        if (child.object != NULL) {
            child.object->panel.bounds.unsignedRect.h = 0x12;
            child.object->panel.bounds.unsignedRect.x = (obj->panel.contentOriginX.unsignedValue + obj->panel.contentRight.unsignedValue + 5) - child.object->panel.bounds.unsignedRect.w;
            child.object->panel.bounds.unsignedRect.y = obj->panel.contentOriginY.unsignedValue + obj->panel.contentBottom.unsignedValue + 8;
            obj->resultValue                          = 0;
            obj->panel.control.word                   = USER_INTERFACE_PANEL_INACTIVE;
        }
        return 0;
    }
    child.object = child.task->spawnArg2.pointer;
    if (child.object->result == USER_INTERFACE_RESULT_CONFIRM) {
        obj->resultValue = child.object->resultValue;
        Ui_TeardownTree(child.object, child.object->owner);
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

static inline void Mc_UpdateTitleHeaderChecksum(void)
{
    u16 sum;
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
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.headerChecksumComplement = 0xFFFF - (u32)sum;
    Mc_VerifySaveHdrChecksum(&gMcSaveData[MEMORY_CARD_SAVE_LIVE]);
}

static inline u8* Mc_CopyTitleBytes(u8* src, u8* dst)
{
    while (*src != 0) {
        *dst++ = *src++;
    }
    *dst = 0;
    return dst;
}

static inline void Mc_UpdateTitleDataChecksum(void)
{
    u16              sum;
    s32              count;
    u8*              src;
    McChecksumBlock* dst;
    s32              i;

    sum                     = 0;
    count                   = 0x200;
    src                     = Mc_DefaultChecksumSrc;
    dst                     = (McChecksumBlock*)&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.titleChecksum;
    i                       = 0;
    dst->checksum           = sum;
    dst->checksumComplement = 0xFFFF - (u32)sum;
    do {
        i   += 1;
        sum += (s8)*src;
        src += 1;
    } while (i < count);
    dst->checksum           = sum;
    dst->checksumComplement = 0xFFFF - (u32)sum;
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
    title                                               = Mc_EncodeTitleText((s8*)Text_FormatTime(buffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime), title);
    title                                               = Mc_EncodeTitleLiteral(" ", title);
    Mc_DefaultChecksumSrc[0x43]                         = 0;
    Mc_DefaultChecksumSrc[0x42]                         = 0;
    title                                               = (u16*)Mc_CopyTitleBytes(Mc_LocationTitleLabels[(s8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.savePoint], (u8*)title);
    title                                               = Mc_EncodeTitleLiteral("(", title);
    title                                               = Mc_EncodeTitleText((s8*)Text_ItoaSigned(buffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveNumber), title);
    title                                               = Mc_EncodeTitleLiteral((s8*)McText_CloseParen, title);
    *title                                              = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount == 0xFF) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount = 0;
    } else if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount < 99) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount++;
    }
    Mc_UpdateTitleHeaderChecksum();
    Mc_UpdateTitleDataChecksum();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksum           = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksumComplement = 0xFFFF;
}

static const char McText_CloseParen[] = ")";

static const McStateFuncTable44 Mc_PromptStates = { {
    Mc_ResetWork,
    Mc_WriteSlotChecksumsEx,
    Mc_StateAcceptMode1,
    Mc_StateSyncAdvance,
    Mc_StateCompareBuffers,
    Mc_StateDrawPromptAdvance,
    Mc_StateOpenRead,
    Mc_StatePromptChoiceB,
    Mc_StateDrawPrompt4,
    Mc_StateCreateFile,
    Mc_StateEnterDialog4,
    Mc_StateWriteFile,
    Mc_StateSyncAdvance,
    Mc_StatePadFileName,
    Mc_StatePromptChoiceGeneric,
    Mc_StateBackupBuffers,
    Mc_StateWriteData,
    Mc_StateSyncAdvance,
    Mc_StateFreeBuffer,
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
    Mc_StateOpenSelected,
    Mc_StateReadHeader,
    Mc_StateSyncAdvance,
    Mc_StateOpenNext,
    Mc_StateFileSelect,
    Mc_StateUiCountdown2,
    Mc_StateUiCountdownF,
    Mc_StateUiCountdownE,
    Mc_StateEnterPromptE,
    Mc_StateEnterPromptD,
} };

static void Mc_StateScanDirFlags(Task* task, McWork* work)
{
    s32       ret;
    UiObject* obj;
    s32       i;
    s32       j;
    s32       size;
    s32       blocks;
    s32       head;
    s32       idx;

    work->field_4 -= 1;
    if (work->field_4 == 0) {
        work->entryCount = 0;
        /* Mark every block free, then claim the 8KB blocks each file spans. */
        for (i = 0; i < 15; i++) {
            work->blockOwners[i] = -1;
        }
        MemCardGetDirentry(
            work->field_C, "*", work->directory, &work->entryCount, 0,
            0xF);

        work->freeBlockCount = 0;
        if (work->entryCount != 0) {
            for (i = 0; i < work->entryCount; i++) {
                size   = work->directory[i].size;
                head   = work->directory[i].head;
                blocks = size / 0x2000 + ((size % 0x2000) != 0);
                head  /= 64;
                head  -= 1;
                for (j = 0; j < blocks; j++) {
                    work->blockOwners[head + j] = -2;
                }
                work->freeBlockCount += blocks;
            }
        }
        task->state += 1;
    }

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, Mc_PromptTable[idx].field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, Mc_PromptTable[idx].field_4, ret, 1, 0);
}

static void Mc_StateListDirectory(Task* task, McWork* work)
{
    s32           ret;
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
        work->field_C, (char*)Mc_SaveFilePattern, work->directory, &work->entryCount, 0,
        0xF);
    temp_v0              = work->freeBlockCount - work->entryCount;
    work->freeBlockCount = temp_v0;
    if (temp_v0 == 0xF) {
        var_v0 = 0x19;
    } else {
        if (work->field_28 == -1) {
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
            head                   /= 64;
            head                   -= 1;
            work->blockOwners[head] = i;
        }
    }

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, one, 0);
}

/// Inline form of Mc_DrawPrompt.
static inline void _mcDrawPrompt(Task* task, s32 mode)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[mode];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

/// Tear down the task's child UI and report status on the task's own object.
static inline void _mcCloseChild(Task* task, s32 status)
{
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    child = task->firstChild;
    if (child != NULL) {
        obj                     = child->spawnArg2.pointer;
        flag                    = task->spawnArg2.pointer;
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        Ui_TeardownTree(obj, obj->owner);
        flag->panel.control.word = status;
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
    work->promptId = 0x16;
    _mcDrawPrompt(task, 0x16);

    child = task->firstChild;
    if (child == NULL) {
        if (Ui_SpawnFromDesc(Mc_LoadListDescriptors, work, 1, 2, obj) != 0) {
            Mc_LoadSlotList.field_4 = work->entryCount;
            if (work->entryCount < 0xF - work->freeBlockCount) {
                Mc_LoadSlotList.field_4++;
            }
            Mc_LoadSlotList.field_10 = work->selectedSlot;
            obj->resultValue         = 0;
            obj->panel.control.word  = USER_INTERFACE_PANEL_INACTIVE;
        }
    } else {
        childObj = child->spawnArg2.pointer;
        if (childObj->result == USER_INTERFACE_RESULT_CONFIRM) {
            obj->resultValue             = childObj->resultValue;
            childObj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            Ui_TeardownTree(childObj, childObj->owner);
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
                        work->field_28 = -1;
                    }
                    work->promptId = 1;
                    task->state    = 5;
                } else {
                    _mcCopyFileName(0);
                    Mc_BuildFileName(Mc_FileName, obj->resultValue);
                    work->promptId = 1;
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
            _mcCloseChild(task, syncResult);
        }
    } else {
        MemCardExist(work->field_C);
    }
}

/// Mask of the buffer slots to write, bit n for slot n: set where the slot's
/// two halves differ, and always for slots 0, 1 and 8.
static inline s32 _mcCompareBufferHalves(void)
{
    McBufferSlot* base;
    u8*           src;
    u8*           dest;
    u32           count;
    u32           i;
    u32           j;
    s32           flags;

    flags = 0;
    i     = 0;
    base  = Mc_BufferSlots;
    do {
        src   = (u8*)base[8 - i].buffer;
        count = base[8 - i].bytesPerCopy;
        j     = 0;
        dest  = src + count;
        while (j < count) {
            if (*src != *dest) {
                flags |= 1;
            }
            j    += 1;
            src  += 1;
            dest += 1;
        }
        i      += 1;
        flags <<= 1;
    } while (i < 8U);
    flags |= 0x100;
    flags |= 3;
    return flags;
}

static void Mc_StateCompareBuffers(Task* task, McWork* work)
{
    s32           flags;
    s32           ret;
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
            work->field_24 = 9;
            flags          = _mcCompareBufferHalves();
            work->field_2C = 1;
            work->field_28 = flags;
            task->state    = 0x1F;
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
            *ptr0          = 0;
            *ptr1          = 0;
            work->field_24 = 9;
            work->field_28 = -1;
            work->field_2C = 1;
            task->state    = 0x1F;
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

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, one, 0);
}

static void Mc_StateOpenRead(Task* task, McWork* work)
{
    s32           ret;
    u32           status;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4 -= 1;
    if (work->field_4 == 0) {
        MemCardClose();
        status           = MemCardOpen(work->field_C, Mc_FileName, 2);
        work->syncResult = status;
        switch (status) {
            case 0:
                task->state = 0x1A;
                break;
            case 1:
                task->state = 0x18;
                break;
            case 2:
                task->state = 0x18;
                break;
            case 3:
                task->state = 0x18;
                break;
            case 4:
                task->state = 0x18;
                break;
            case 5:
                task->state = 0x7;
                break;
            default:
                task->state = 0x18;
                break;
        }
    }

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateCreateFile(Task* task, McWork* work)
{
    s32           ret;
    u32           status;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4 -= 1;
    if (work->field_4 == 0) {
        status           = MemCardCreateFile(work->field_C, Mc_FileName, 1);
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

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StatePadFileName(Task* task, McWork* work)
{
    s32           ret;
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
            work->field_24 = 9;
            work->field_28 = -1;
            work->field_2C = 0;
            task->state    = 5;
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

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateNameEntry(Task* task, McWork* work)
{
    s32 syncResult;
    u8* src;
    u8* dst;
    s32 i;

    if (work->field_2C == 1) {
        work->promptId = 0x11;
        switch (Mc_PromptDialogSpawn(task, 0x11, work->field_0)) {
            case 0:
                break;
            case 1:
                work->field_4      = 0xE;
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
                _mcCloseChild(task, syncResult);
            }
        } else {
            MemCardExist(work->field_C);
        }
    } else {
        work->sectorOffset = 0;
        work->promptId     = 4;
        task->state        = 0xF;
        _mcDrawPrompt(task, work->promptId);
    }
}

/// Copy the first half of each of Mc_BufferSlots[1..8] over its second half.
static inline void _mcCopyBufferHalves(void)
{
    McBufferSlot* p;
    McBufferSlot* base;
    u8*           src;
    u8*           dest;
    u32           count;
    u32           i;
    u32           j;

    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        src   = (u8*)p->buffer;
        count = p->bytesPerCopy;
        j     = 0;
        dest  = src + count;
        while (j < count) {
            j      += 1;
            *dest++ = *src++;
        }
        i += 1;
        p += 1;
    } while (i < 9U);
}

/// Inline form of Mc_WriteFirstByteChecksum.
static inline void _mcWriteFirstByteChecksum(void)
{
    McChecksumBlock* temp;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              next;
    s16              sum;
    u32              i;

    sum  = 0;
    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        temp = p->buffer;
        p   += 1;
        i   += 1;
        next = sum + *(u8*)temp;
        sum  = next;
    } while (i < 9U);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksum           = next;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksumComplement = ~next;
}

static void Mc_StateBackupBuffers(Task* task, McWork* work)
{
    McChecksumBlock* buf;
    s32              size;
    void*            mem;

    if (work->field_24 == 0) {
        _mcCopyBufferHalves();
        work->field_A18 = 0x33;
        task->state     = 0x13;
    } else if (work->field_28 & 1) {
        size                = Mc_BufferSlots[9 - work->field_24].bytesPerCopy;
        buf                 = Mc_BufferSlots[9 - work->field_24].buffer;
        work->transferBytes = (((u32)(size * 2 - 1) >> 7) + 1) << 7;
        mem                 = memCalloc(work->transferBytes, 0);
        if (mem != 0) {
            work->buffer = mem;
            if (work->field_24 == 9) {
                Mc_BuildSaveTitle(work);
                memcpy(mem, buf, size * 2);
            } else {
                _mcWriteBlockChecksum((u8*)buf, size);
                if (work->field_24 == 8) {
                    _mcWriteFirstByteChecksum();
                }
                memcpy(mem, buf, size);
                memcpy((u8*)mem + size, buf, size);
            }
            work->field_4  = 0;
            task->state    = task->state + 1;
            work->field_24 = work->field_24 - 1;
            work->field_28 = (u32)work->field_28 >> 1;
        } else {
            work->field_4 = work->field_4 + 1;
        }
    } else {
        work->sectorOffset = work->sectorOffset + Mc_BufferSlots[9 - work->field_24].cardSectors;
        work->field_24     = work->field_24 - 1;
        work->field_28     = (u32)work->field_28 >> 1;
    }

    work->promptId = 4;
    _mcDrawPrompt(task, 4);
}

static void Mc_StateFreeBuffer(Task* task, McWork* work)
{
    s32           ret;
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
            work->sectorOffset += Mc_BufferSlots[8 - work->field_24].cardSectors;
            task->state         = 0xF;
            break;
        case 1:
            MemCardClose();
            task->state = 0x14;
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
            *ptr0 = 0;
            *ptr1 = 0;
            MemCardClose();
            task->state = 0x2;
            break;
        case 2:
        case 4:
        case 5:
        default:
            task->state = 0x2A;
            break;
    }
    memFree(work->buffer);
    work->buffer = 0;

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateFormat(Task* task, McWork* work)
{
    s32           ret;
    s32           status;
    s32           idx;
    s32           next;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4 -= 1;
    if (work->field_4 == 0) {
        status           = MemCardFormat(work->field_C);
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

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
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
    work->promptId = 0x16;
    _mcDrawPrompt(task, 0x16);

    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    if (syncResult == -1) {
        MemCardExist(work->field_C);
    } else if (syncResult == 1 && work->syncResult != 0) {
        task->state = 7;
        _mcCloseChild(task, syncResult);
        return;
    }
    child = task->firstChild;
    if (child == NULL) {
        if (Ui_SpawnFromDesc(Mc_SaveListDesc, work, 1, 2, obj) != 0) {
            Mc_SaveSlotList.field_4 = work->entryCount;
            obj->resultValue        = 0;
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    } else {
        childObj = child->spawnArg2.pointer;
        if (childObj->result == USER_INTERFACE_RESULT_CONFIRM) {
            obj->resultValue             = childObj->resultValue;
            childObj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            Ui_TeardownTree(childObj, childObj->owner);
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

/// Jump table of 26 McStateFunc handlers used by Mc_DispatchStateTable26.
static const McStateFuncTable26 Mc_FileSelectStates = { {
    Mc_StateInitWorkDefaults,
    Mc_StateSetOpenDefaults,
    Mc_StateCountdownPrompt,
    Mc_StateCloseReturn,
    Mc_StatePromptTimeout,
    Mc_KillIfCountdownAlt,
    Mc_StateEnterPromptF,
    Mc_StateAccept,
    Mc_StateSyncAdvance,
    Mc_StateBlankFileName,
    Mc_StateSyncPrompt3,
    Mc_StateSyncPromptA,
    Mc_StateDrawCurrentPrompt,
    Mc_StateSyncOpen,
    Mc_StateVerifyFinish,
    Mc_StateReadData,
    Mc_StateSyncAdvance,
    Mc_StateFinishWrite,
    Mc_StateDrawPrompt1,
    Mc_StateGetDirentry,
    Mc_StateOpenDirEntry,
    Mc_StateReadSlot,
    Mc_StateSyncAdvance,
    Mc_StateWalkDirectory,
    Mc_StateSyncFileSelect,
    Mc_StateEnterPrompt17,
} };

static void Mc_StateBlankFileName(Task* task, McWork* work)
{
    s32           ret;
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

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateSyncOpen(Task* task, McWork* work)
{
    s32   syncResult;
    u32   status;
    char* fileName;

    _mcDrawPrompt(task, work->promptId);

    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (work->syncCommand == syncResult) {
                if (work->syncResult != 0) {
                    task->state = 6;
                }
            }
            work->field_4 -= 1;
            if (work->field_4 == 0) {
                fileName = Mc_FileName;
                MemCardClose();
                status           = MemCardOpen(work->field_C, fileName, 1);
                work->syncResult = status;
                switch (status) {
                    case 0:
                        work->sectorOffset = 0;
                        task->state        = 0xE;
                        break;
                    case 1:
                    case 2:
                        task->state = 6;
                        break;
                    case 5:
                        task->state = 0xB;
                        break;
                    case 3:
                    case 4:
                    default:
                        task->state = 6;
                        break;
                }
            }
        }
    } else {
        MemCardExist(work->field_C);
    }
}

/// Inline form of Mc_VerifySlotChecksums.
static inline s32 _mcVerifySlotChecksums(void)
{
    McChecksumBlock* block;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              sum;
    u32              count;
    u32              i;
    u32              j;
    u8*              ptr;
    s32              ok;

    ok   = 1;
    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        sum   = 0;
        block = p->buffer;
        count = p->bytesPerCopy;
        ptr   = block->payload;
        count = count - 4;
        j     = 0;
        while (j < count) {
            j   += 1;
            sum += (s8)*ptr++;
        }
        if ((u16)block->checksum != (sum & 0xFFFF)) {
            ok = 0;
        }
        i += 1;
        p += 1;
    } while (i < 9U);
    return ok;
}

/// Inline form of Mc_WriteSlotChecksums: store the signed byte sum of each
/// buffer slot 1..8's payload, and its complement, in the buffer's header.
static inline void _mcWriteSlotChecksums(void)
{
    McChecksumBlock* block;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              sum;
    s32              inv;
    u32              count;
    u32              i;
    u32              j;
    u8*              ptr;

    i    = 1;
    inv  = 0xFFFF;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        sum   = 0;
        j     = 0;
        block = p->buffer;
        count = p->bytesPerCopy;
        ptr   = block->payload;
        count = count - 4;
        while (j < count) {
            j   += 1;
            sum += (s8)*ptr++;
        }
        p                        += 1;
        i                        += 1;
        block->checksumComplement = inv - sum;
        block->checksum           = sum;
    } while (i < 9U);
}

/// Inline form of Mc_VerifyFirstByteChecksum.
static inline s32 _mcVerifyFirstByteChecksum(void)
{
    s32           sum;
    u32           i;
    McBufferSlot* p;
    McBufferSlot* base;

    sum  = 0;
    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        sum += *(u8*)p->buffer;
        p   += 1;
        i   += 1;
    } while (i < 9);
    return ((u16)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.bufferChecksum ^ (sum & 0xFFFF)) == 0;
}

static void Mc_StateVerifyFinish(Task* task, McWork* work)
{
    s32   size;
    void* mem;

    if (work->field_24 == 0) {
        if (_mcVerifySlotChecksums() && _mcVerifyFirstByteChecksum()) {
            Game_ClearEd68();
            gDisplayState.control.flags.pendingPlayerPos = 1;
            task->state                                  = 3;
        } else {
            Mc_InitBufferSlots();
            task->state = 0x19;
        }
    } else if (work->field_28 & 1) {
        size                = Mc_BufferSlots[9 - work->field_24].bytesPerCopy;
        size              <<= 1;
        size               -= 1;
        size                = (u32)size >> 7;
        size               += 1;
        size              <<= 7;
        work->transferBytes = size;
        mem                 = Mem_Malloc(size, 0);
        work->buffer        = mem;
        if (mem != 0) {
            work->field_4  = 0;
            task->state    = task->state + 1;
            work->field_24 = work->field_24 - 1;
            work->field_28 = (u32)work->field_28 >> 1;
        } else {
            work->field_4 = work->field_4 + 1;
        }
    } else {
        work->sectorOffset = work->sectorOffset + Mc_BufferSlots[9 - work->field_24].cardSectors;
        work->field_24     = work->field_24 - 1;
        work->field_28     = (u32)work->field_28 >> 1;
    }

    work->promptId = 5;
    _mcDrawPrompt(task, 5);
}

/// Checksum the 0x200-byte work buffer into the work's sum / complement pair,
/// clearing the pair first.
static inline void _mcWriteWorkChecksum(McWork* work)
{
    s16  sum;
    s32  count;
    u8*  src;
    u16* dst;
    s32  i;

    sum    = 0;
    count  = 0x200;
    src    = work->buffer;
    dst    = &work->checksum;
    i      = 0;
    dst[0] = 0;
    dst[1] = ~0;
    do {
        i   += 1;
        sum += (s8)*src;
        src += 1;
    } while (i < count);
    dst[0] = sum;
    dst[1] = ~sum;
}

static void Mc_StateFinishWrite(Task* task, McWork* work)
{
    u32 status;
    s32 slotIdx;
    s32 size;
    s32 i;
    s32 ch;
    u8* ptr1;
    u8* ptr0;

    status = work->syncResult;
    if (status < 4U) {
        if (status == 0) {
            slotIdx = 8 - work->field_24;
            if (slotIdx == 0) {
                _mcWriteWorkChecksum(work);
            } else {
                size   = Mc_BufferSlots[slotIdx].bytesPerCopy;
                size <<= 1;
                memcpy(Mc_BufferSlots[slotIdx].buffer, work->buffer, size);
            }
            work->sectorOffset += Mc_BufferSlots[8 - work->field_24].cardSectors;
            task->state         = 0xE;
        } else {
            goto pad;
        }
    } else {
    pad:
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
        task->state = 6;
    }

    memFree(work->buffer);
    work->buffer = 0;
    _mcDrawPrompt(task, work->promptId);
}

/// Inline form of Mc_VerifySaveHdrChecksum: whether a save header names a valid
/// save point and carries the checksum of its 56 bytes from `McSavePreview.location`.
static inline s32 _mcVerifySaveHdrChecksum(McSavePreview* save)
{
    u16 sum;
    u8* ptr;
    s32 limit;
    s32 i;

    sum = 0;
    if ((u32)(save->savePoint - 1) >= (u32)MEMORY_CARD_SAVE_POINT_COUNT) {
        return 0;
    }
    // The serialized checksum spans fields and retained bytes beyond the location cell.
    ptr   = (u8*)save + OFFSET_OF(McSavePreview, location);
    limit = MEMORY_CARD_SAVE_HEADER_CHECKSUM_BYTES;
    i     = 0;
    do {
        i   += 1;
        sum += (s8)*ptr;
        ptr += 1;
    } while (i < limit);
    return save->headerChecksum == sum;
}

static void Mc_StateSaveSlotUi(UiList* list, UiObject* object)
{
    McWork* base;
    s32     previewByteOffset;
    s32     enabled;

    enabled           = 1;
    previewByteOffset = list->field_8 * sizeof(McSavePreview) + OFFSET_OF(McWork, previews);
    base              = object->owner->spawnArg1.pointer;
    if (!_mcVerifySaveHdrChecksum((McSavePreview*)((u8*)base + previewByteOffset))) {
        enabled = 0;
        Ui_LookupTable(object, 2);
    }
    Mc_DrawSlotDetails(object, base, list->field_8, 0, list->field_1A + 7);
    if (list->field_C == 1) {
        if (enabled && Pad_CheckButtons(0, 1, Pad_MaskConfirm)) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            object->result      = USER_INTERFACE_RESULT_CONFIRM;
            object->resultValue = list->field_8;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel)) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
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
    s32            color;
    McSavePreview* save;

    color = Ui_LookupTable(object, 1);
    if (slot < work->entryCount) {
        save = &work->previews[slot];
        if (!_mcVerifySaveHdrChecksum(save)) {
            x = arg3 + object->panel.contentLeft.signedValue + 8;
            y = arg4 + object->panel.contentTop.signedValue + 0x11;
            if (work->field_A20 == 0) {
                Text_DrawPrompt(object, x, y, McText_LoadAbortedCorrupted, 0x606060, 1, 0);
                return;
            }
            Text_DrawMultiLine(object, x, y, McText_LoadAbortedCorrupted, 0x37A78, 1, 0);
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
        Text_DrawString(&timeLabelRequest, McText_Time);
        timeValueRequest.x          = object->panel.contentOriginX.unsignedValue + 0x28 + x;
        timeValueRequest.y          = object->panel.contentOriginY.unsignedValue + y;
        timeValueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        timeValueRequest.colorRgb   = color;
        timeValueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        timeValueRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        timeValueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        Text_DrawString(&timeValueRequest, Text_FormatTime(sp20.buf, save->playTime));
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
            Text_DrawString(&sp60.req, McText_Clear);
            detailRequest.x          = object->panel.contentOriginX.unsignedValue + x;
            detailRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            detailRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            detailRequest.colorRgb   = color;
            detailRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            detailRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
            detailRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            Text_DrawString(&detailRequest, Text_ItoaUnsigned(sp20.buf, save->clearCount));
            if ((s8)save->savePoint != MEMORY_CARD_SAVE_POINT_OPENING) {
                statRequest.x          = object->panel.contentOriginX.unsignedValue + x;
                statRequest.y          = object->panel.contentOriginY.unsignedValue + 8 + y;
                statRequest.otIndex    = object->panel.otIndex.signedValue + 1;
                statRequest.colorRgb   = 0x606060;
                statRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
                statRequest.alignment  = TEXT_ALIGNMENT_RIGHT;
                statRequest.drawMode   = TEXT_DRAW_OUTLINED;
                Text_DrawString(&statRequest, Mc_ModeLabels[save->gameMode]);
            }
        }
        x = arg3 + object->panel.contentLeft.signedValue + 4;
        y = arg4 + object->panel.contentTop.signedValue + 0x11;
        Text_DrawPrompt(object, x, y, Mc_LocationLabels[(s8)save->savePoint], color, 1, 0);
        sp60.buf[0] = 0;
        Text_Strcat(sp60.buf, McText_OpenParen);
        Text_Strcat(sp60.buf, Text_ItoaSigned(sp20.buf, save->saveNumber));
        Text_Strcat(sp60.buf, McText_CloseParen);
        detailRequest.x          = object->panel.contentOriginX.unsignedValue + (x + Text_MeasureWidth(Mc_LocationLabels[(s8)save->savePoint]));
        detailRequest.y          = object->panel.contentOriginY.unsignedValue + (y - 3);
        detailRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        detailRequest.colorRgb   = color;
        detailRequest.glyphTable = TEXT_GLYPH_TABLE_LARGE;
        detailRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        detailRequest.drawMode   = TEXT_DRAW_OUTLINED;
        Text_DrawString(&detailRequest, sp60.buf);
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
        Text_DrawString(&detailRequest, McText_Exp);
        if ((s8)save->savePoint != MEMORY_CARD_SAVE_POINT_OPENING) {
            statRequest.x          = object->panel.contentOriginX.unsignedValue + x;
            statRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            statRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            statRequest.colorRgb   = 0x606060;
            statRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            statRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            statRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            Text_DrawString(&statRequest, Text_ItoaSigned(sp20.buf, save->playerExp));
        } else {
            statRequest.x          = object->panel.contentOriginX.unsignedValue + x;
            statRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            statRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            statRequest.colorRgb   = 0x606060;
            statRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            statRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            statRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            Text_DrawString(&statRequest, McText_Unavailable);
        }
        x                      = arg3 - 0x28;
        statRequest.x          = object->panel.contentOriginX.unsignedValue + x;
        statRequest.y          = object->panel.contentOriginY.unsignedValue + (y - 2);
        statRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        statRequest.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        statRequest.colorRgb   = 0x606060;
        statRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        statRequest.drawMode   = TEXT_DRAW_OUTLINED;
        Text_DrawString(&statRequest, McText_Bp);
        if ((s8)save->savePoint != MEMORY_CARD_SAVE_POINT_OPENING) {
            bpValueRequest.x          = object->panel.contentOriginX.unsignedValue + 0x1E + x;
            bpValueRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            bpValueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            bpValueRequest.colorRgb   = 0x606060;
            bpValueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            bpValueRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            bpValueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            Text_DrawString(&bpValueRequest, Text_ItoaSigned(sp20.buf, save->playerBp));
        } else {
            bpValueRequest.x          = object->panel.contentOriginX.unsignedValue + 0x1E + x;
            bpValueRequest.y          = object->panel.contentOriginY.unsignedValue + y;
            bpValueRequest.otIndex    = object->panel.otIndex.signedValue + 1;
            bpValueRequest.colorRgb   = 0x606060;
            bpValueRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            bpValueRequest.alignment  = TEXT_ALIGNMENT_LEFT;
            bpValueRequest.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            Text_DrawString(&bpValueRequest, McText_Unavailable);
        }
    } else {

        sp20.req.x          = object->panel.contentOriginX.unsignedValue + arg3;
        sp20.req.y          = object->panel.contentOriginY.unsignedValue + 5 + arg4;
        sp20.req.otIndex    = object->panel.otIndex.signedValue + 1;
        sp20.req.glyphTable = TEXT_GLYPH_TABLE_LARGE;
        sp20.req.colorRgb   = color;
        sp20.req.alignment  = TEXT_ALIGNMENT_CENTER;
        sp20.req.drawMode   = TEXT_DRAW_OUTLINED;
        Text_DrawString(&sp20.req, McText_NewBlock);
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
    Mc_VerifySaveHdrChecksum(&gMcSaveData[MEMORY_CARD_SAVE_LIVE]);
}

static s32 Mc_VerifySaveHdrChecksum(McSaveData* save)
{
    return _mcVerifySaveHdrChecksum(&save->preview);
}

/// Out-of-line form of `_mcWriteBlockChecksum`. Nothing calls it.
static void Mc_WriteBlockChecksum(u8* data, s32 size)
{
    McChecksumBlock* block;
    s16              sum;
    u32              i;

    block = (McChecksumBlock*)data;
    sum   = 0;
    data  = block->payload;
    size -= 4;
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

void Mc_ResetSaveFlags(void)
{
    McSaveData* p;

    p                     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    p->state.vibration    = 0;
    p->state.buttonLayout = 0;
    p->state.musicVolume  = 0;
    p->state.cursorMode   = 0;
    p->state.soundMode    = 0;
    p->state.moveMode     = 0;
    CdVol_SetMixMode(1);
    Snd_ApplyVolumeTable(0);
}

static void Mc_ClearWorkBuffers(void)
{
    GameFlagAcropolisBank*    a;
    GameFlagDryfieldBank*     b;
    GameFlagDryfieldFullBank* c;
    GameFlagShelterBank*      d;
    GameFlagNeoArkBank*       e;

    a = GameFlag_AcropolisBanks;
    Mem_Set(a, 0, 0x6C);
    b = GameFlag_DryfieldBanks;
    Mem_Set(b, 0, 0xB0);
    c = GameFlag_DryfieldFullBanks;
    Mem_Set(c, 0, 0x24);
    d = GameFlag_ShelterBanks;
    Mem_Set(d, 0, 0xE4);
    e = GameFlag_NeoArkBanks;
    Mem_Set(e, 0, 0xA4);
    Mem_Set(a + 1, 0xFF, 0x6C);
    Mem_Set(b + 1, 0xFF, 0xB0);
    Mem_Set(c + 1, 0xFF, 0x24);
    Mem_Set(d + 1, 0xFF, 0xE4);
    Mem_Set(e + 1, 0xFF, 0xA4);
}

// TODO
void Mc_InitLib(void)
{
    MemCardInit(0); // 0 = No control routine
    MemCardStart();
    Mc_InitBufferSlots();
}

/// Whether a buffer's header holds the sum of its payload, as
/// `Mc_WriteBlockChecksum` stores it. Only the sum is compared, not its
/// complement. Nothing calls it.
static s32 Mc_VerifyBlockChecksum(u8* data, s32 size)
{
    McChecksumBlock* block;
    s16              sum;
    u32              i;

    block = (McChecksumBlock*)data;
    sum   = 0;
    data  = block->payload;
    size -= 4;
    i     = 0;
    if (size != 0) {
        do {
            i    += 1;
            sum  += (s8)*data;
            data += 1;
        } while (i < size);
    }
    return ((u16)block->checksum ^ (sum & 0xFFFF)) == 0;
}

static void Mc_UnusedStub(void)
{
}

static s32 Mc_CompareBufferHalves(void)
{
    return _mcCompareBufferHalves();
}

static void Mc_WriteSlotChecksums(void)
{
    _mcWriteSlotChecksums();
}

static void Mc_WriteFirstByteChecksum(void)
{
    _mcWriteFirstByteChecksum();
}

static s32 Mc_VerifyFirstByteChecksum(void)
{
    return _mcVerifyFirstByteChecksum();
}

static s32 Mc_VerifySlotChecksums(void)
{
    return _mcVerifySlotChecksums();
}

static void Mc_DuplicateBuffers(void)
{
    u32           i;
    u32           j;
    McBufferSlot* p;
    McBufferSlot* base;
    u8*           src;
    s32           size;
    u8*           dest;

    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        src  = (u8*)p->buffer;
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
        Ui_TeardownTree(obj, obj->owner);
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
    work->field_0   = 0x10;
    work->field_4   = 0;
    work->buffer    = 0;
    work->field_C   = 0;
    work->field_A18 = 0x34;
    work->field_A20 = 0;
    task->state++;
}

static void Mc_WriteSlotChecksumsEx(Task* task, McWork* work)
{
    work->field_24 = 9;
    work->field_28 = -1;
    work->field_2C = 1;
    _mcWriteSlotChecksums();

    if (task->spawnArg1.value != 0) {
        task->killCountdown = 2;
        task->state         = 0x27;
    } else {
        task->state = 0xE;
    }
}

static void Mc_StateAcceptMode1(Task* task, McWork* work)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->promptId = 1;
    if (MemCardAccept(work->field_C) != 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        work->field_4 = work->field_4 + 1;
    }
    work->field_4 = work->field_4 + 1;
    idx           = work->promptId;
    obj           = task->spawnArg2.pointer;
    ret           = Ui_LookupTable(obj, 1);
    obj->result   = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    if (work->field_0 > 0) {
        work->field_0 -= 2;
    }
    if (work->field_0 < 0) {
        work->field_0 += 2;
    }
}

static void Mc_StateSyncAdvance(Task* task, McWork* work)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    if (MemCardSync(1, &work->syncCommand, &work->syncResult) != 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        work->field_4 = work->field_4 + 1;
    }
    idx         = work->promptId;
    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    if (work->field_0 > 0) {
        work->field_0 -= 2;
    }
    if (work->field_0 < 0) {
        work->field_0 += 2;
    }
}

static void Mc_StateDrawPromptAdvance(Task* task, McWork* work)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4 = 0xE;
    obj           = task->spawnArg2.pointer;
    idx           = work->promptId;
    ret           = Ui_LookupTable(obj, 1);
    obj->result   = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
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

    work->promptId = 0xB;
    ret            = Mc_PromptDialogChoice(task, 0xB, work->field_0);
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
                    Ui_TeardownTree(obj, obj->owner);
                    flag->panel.control.word = syncResult;
                }
            }
        }
    } else {
        MemCardExist(work->field_C);
    }
}

static void Mc_StateDrawPrompt4(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4  = 0xE;
    work->promptId = 4;
    obj            = task->spawnArg2.pointer;
    ret            = Ui_LookupTable(obj, 1);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    task->state = task->state + 1;
}

static void Mc_StateEnterDialog4(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4 = 0;
    task->state++;
    work->promptId = 4;
    obj            = task->spawnArg2.pointer;
    ret            = Ui_LookupTable(obj, 1);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateWriteFile(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardWriteFile(work->field_C, Mc_FileName, (unsigned long*)Mc_DefaultChecksumSrc, 0,
                         0x200) != 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        work->field_4 = work->field_4 + 1;
    }
    idx         = work->promptId;
    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StatePromptChoiceGeneric(Task* task, McWork* work)
{
    s32 ret;

    work->promptId = 7;
    ret            = Mc_PromptDialogChoice(task, 7, work->field_0);
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
    if (work->field_0 > 0) {
        work->field_0 -= 2;
    }
    if (work->field_0 < 0) {
        work->field_0 += 2;
    }
}

static void Mc_StateWriteData(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardWriteData(work->buffer, work->sectorOffset << 7, work->transferBytes) != 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        work->field_4 = work->field_4 + 1;
    }
    idx         = work->promptId;
    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateClosePrompt(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;
    UiObject*     flag;
    s16           val;

    MemCardClose();
    idx         = work->promptId;
    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    task->state = 0x1B;
    flag        = task->spawnArg2.pointer;
    if (flag != NULL) {
        val               = work->field_A18;
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

    work->promptId = 3;
    if (Mc_PromptDialogFile(task, 3, work->field_0) != 0) {
        task->state = 0x13;
        return;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    switch (syncResult) {
        case -1:
            MemCardExist(work->field_C);
            return;
        case 1:
            if (work->syncResult != syncResult) {
                child = task->firstChild;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    Ui_TeardownTree(obj, obj->owner);
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

    work->promptId = 9;
    ret            = Mc_PromptDialogSpawn(task, 9, work->field_0);
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
                    Ui_TeardownTree(obj, obj->owner);
                    flag->panel.control.word = syncResult;
                }
            }
        }
    } else {
        MemCardExist(work->field_C);
    }
}

static void Mc_StateColdBoot(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->promptId = 6;
    obj            = task->spawnArg2.pointer;
    ret            = Ui_LookupTable(obj, 1);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[6];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    work->field_4 = 0xE;
    task->state   = task->state + 1;
}

static void Mc_StateSyncPrompt13(Task* task, McWork* work)
{
    s32       syncResult;
    s32       rslt;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    work->promptId = 0x13;
    if (Mc_PromptDialogFile(task, 0x13, work->field_0) != 0) {
        task->state = 0x13;
        return;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    switch (syncResult) {
        case -1:
            MemCardExist(work->field_C);
            return;
        case 1:
            rslt = work->syncResult;
            if (rslt == syncResult) {
                child = task->firstChild;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    Ui_TeardownTree(obj, obj->owner);
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

    work->promptId = 0;
    work->field_4  = 0;
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
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    work->field_0 -= 1;
    obj            = task->spawnArg2.pointer;
    idx            = work->promptId;
    ret            = Ui_LookupTable(obj, 1);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    if (work->field_0 < -0x10) {
        task->killCountdown = 0;
        task->state         = -1;
    }
}

static void Mc_StateDrawPromptTo1F(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    idx         = work->promptId;
    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    task->state = 0x1F;
}

static void Mc_StateCountdownPrompt4(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->promptId = 4;
    obj            = task->spawnArg2.pointer;
    ret            = Ui_LookupTable(obj, 1);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    if (work->field_4-- <= 0) {
        work->field_A18 = 0x33;
        task->state     = 0x13;
    }
}

static void Mc_StateDrawPrompt1Advance(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4  = 4;
    work->promptId = 1;
    obj            = task->spawnArg2.pointer;
    ret            = Ui_LookupTable(obj, 1);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[1];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    task->state = task->state + 1;
}

static void Mc_StateOpenSelected(Task* task, McWork* work)
{
    s32 openIdx;
    s32 openResult;

    openIdx = work->currentSlot;
    MemCardClose();
    openResult       = MemCardOpen(work->field_C, work->directory[openIdx].name, 1);
    work->syncResult = openResult;
    if (openResult == 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        task->state = 0x18;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateReadHeader(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData((u_long*)&work->previews[work->currentSlot], MEMORY_CARD_SAVE_PREVIEW_FILE_OFFSET,
                        sizeof(work->previews[work->currentSlot])) != 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        work->field_4 = work->field_4 + 1;
    }
    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateOpenNext(Task* task, McWork* work)
{
    McWork*       a1;
    Task*         a0;
    UiObject*     obj;
    s32           modeIdx;
    s32           ret;
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
    obj         = a0->spawnArg2.pointer;
    modeIdx     = a1->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateUiCountdown2(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    task->killCountdown -= 1;
    if (task->killCountdown <= 0) {
        task->state = 2;
    }
    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateUiCountdownE(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    task->killCountdown -= 1;
    if (task->killCountdown <= 0) {
        task->state = 0xE;
    }
    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateUiCountdownF(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4 -= 1;
    if (work->field_4 <= 0) {
        task->state = 0xF;
    }
    work->promptId = 4;
    obj            = task->spawnArg2.pointer;
    ret            = Ui_LookupTable(obj, 1);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateEnterPromptE(Task* task, McWork* work)
{
    work->promptId = 0xE;
    work->field_4  = 0;
    if (Mc_PromptDialog(task, 0xE, 0) != 0) {
        task->state = 0x13;
    }
}

static void Mc_StateEnterPromptD(Task* task, McWork* work)
{
    work->promptId = 0xD;
    work->field_4  = 0;
    if (Mc_PromptDialog(task, 0xD, 0) != 0) {
        task->state = 0x13;
    }
}

void Mc_DispatchStateTable(Task* task)
{
    McStateFuncTable44 sp;
    McWork*            work;
    s32                state;

    sp    = Mc_PromptStates;
    work  = &Mc_MenuWork;
    state = task->state;
    if (state < 0) {
        Mc_KillIfCountdown(task, work);
        return;
    }
    sp.funcs[state](task, work);
    if (work->field_4 >= 0xB5) {
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
    work->field_0                                = 0x10;
    work->promptId                               = 0x8;
    work->field_A20                              = 1;
    work->field_4                                = 0;
    work->buffer                                 = 0;
    work->field_C                                = 0;
    gDisplayState.control.flags.pendingPlayerPos = 0;
    task->state                                 += 1;
}

static void Mc_StateSetOpenDefaults(Task* task, McWork* work)
{
    work->field_24 = 9;
    work->field_28 = -1;
    task->state    = 7;
}

static void Mc_StateCountdownPrompt(Task* task, McWork* work)
{
    s32           status;
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    if (work->field_0 > 0) {
        work->field_0 -= 2;
    }
    if (work->field_0 == 0) {
        work->promptId = 8;
        status         = Mc_PromptDialogChoice(task, 8, work->field_0);
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
        obj         = task->spawnArg2.pointer;
        idx         = work->promptId;
        ret         = Ui_LookupTable(obj, 1);
        obj->result = USER_INTERFACE_RESULT_NONE;
        Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
        base  = Mc_PromptTable;
        entry = &base[idx];
        Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
        Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    }
}

static void Mc_StateCloseReturn(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    MemCardClose();
    idx         = work->promptId;
    obj         = task->spawnArg2.pointer;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    task->state = 4;
    if (task->spawnArg2.pointer != NULL) {
        ((UiObject*)task->spawnArg2.pointer)->result = USER_INTERFACE_RESULT_CANCEL;
    }
}

static void Mc_StatePromptTimeout(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    work->field_0 -= 1;
    obj            = task->spawnArg2.pointer;
    idx            = work->promptId;
    ret            = Ui_LookupTable(obj, 1);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    if (work->field_0 < -0x10) {
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

    work->promptId = 0xF;
    work->field_4  = 0;
    if (Mc_PromptDialog(task, 0xF, 0) != 0) {
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
    work->promptId = 1;
    if (MemCardAccept(work->field_C) != 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        work->field_4 = work->field_4 + 1;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateSyncPrompt3(Task* task, McWork* work)
{
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    work->promptId = 3;
    if (Mc_PromptDialogFile(task, 3, work->field_0) != 0) {
        task->state = 3;
        return;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    switch (syncResult) {
        case -1:
            MemCardExist(work->field_C);
            return;
        case 1:
            if (work->syncResult != syncResult) {
                child = task->firstChild;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    Ui_TeardownTree(obj, obj->owner);
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

    work->promptId = 0xA;
    if (Mc_PromptDialogFile(task, 0xA, work->field_0) != 0) {
        task->state = 3;
        return;
    }
    syncResult = MemCardSync(1, &work->syncCommand, &work->syncResult);
    switch (syncResult) {
        case -1:
            MemCardExist(work->field_C);
            return;
        case 1:
            rslt = work->syncResult;
            if (rslt == syncResult) {
                child = task->firstChild;
                if (child != NULL) {
                    obj                     = child->spawnArg2.pointer;
                    flag                    = task->spawnArg2.pointer;
                    obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
                    Ui_TeardownTree(obj, obj->owner);
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
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    work->field_4 = 4;
    obj           = task->spawnArg2.pointer;
    idx           = work->promptId;
    ret           = Ui_LookupTable(obj, 1);
    obj->result   = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    task->state = task->state + 1;
}

static void Mc_StateReadData(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData(work->buffer, work->sectorOffset << 7, work->transferBytes) != 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        work->field_4 = work->field_4 + 1;
    }
    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateDrawPrompt1(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4  = 4;
    work->promptId = 1;
    obj            = task->spawnArg2.pointer;
    ret            = Ui_LookupTable(obj, 1);
    obj->result    = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[1];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
    task->state = task->state + 1;
}

static void Mc_StateGetDirentry(Task* task, McWork* work)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    work->field_4 -= 1;
    if (work->field_4 == 0) {
        work->entryCount = 0;
        MemCardGetDirentry(
            work->field_C, (char*)Mc_SaveFilePattern, work->directory, &work->entryCount, 0,
            0xF);
        if (work->entryCount != 0) {
            work->selectedSlot = 0;
            work->currentSlot  = 0;
            task->state        = task->state + 1;
        } else {
            task->state = 0xB;
        }
    }

    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateOpenDirEntry(Task* task, McWork* work)
{
    s32 idx;
    s32 openResult;

    idx = work->currentSlot;
    MemCardClose();
    openResult       = MemCardOpen(work->field_C, work->directory[idx].name, 1);
    work->syncResult = openResult;
    if (openResult == 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        task->state = 6;
    }
    _mcDrawPrompt(task, work->promptId);
}

static void Mc_StateReadSlot(Task* task, McWork* work)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData((u_long*)&work->previews[work->currentSlot], MEMORY_CARD_SAVE_PREVIEW_FILE_OFFSET,
                        sizeof(work->previews[work->currentSlot])) != 0) {
        work->field_4 = 0;
        task->state   = task->state + 1;
    } else {
        work->field_4 = work->field_4 + 1;
    }
    obj         = task->spawnArg2.pointer;
    idx         = work->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateWalkDirectory(Task* task, McWork* work)
{
    McWork*       a1;
    Task*         a0;
    UiObject*     obj;
    s32           modeIdx;
    s32           ret;
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
            a0->state = 0x14;
        } else {
            a0->state = a0->state + 1;
        }
    } else {
        a0->state = 0x6;
    }
    obj         = a0->spawnArg2.pointer;
    modeIdx     = a1->promptId;
    ret         = Ui_LookupTable(obj, 1);
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateEnterPrompt17(Task* task, McWork* work)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    work->promptId = 0x17;
    work->field_4  = 0;
    if (Mc_PromptDialog(task, 0x17, 0) != 0) {
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
    McStateFuncTable26 sp;
    McWork*            work;

    sp   = Mc_FileSelectStates;
    work = &Mc_MenuWork;
    sp.funcs[task->state](task, work);
    if (work->field_4 >= 0xB5) {
        task->state = 6;
    }
}
