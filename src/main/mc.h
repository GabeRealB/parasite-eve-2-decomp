#ifndef MAIN_PRIVATE_MC_H
#define MAIN_PRIVATE_MC_H

#include <psyq/sys/types.h>
#include <psyq/kernel.h>

#include "types.h"

#include "main/mc_types.h"
#include "main/task_types.h"
#include "main/ui_types.h"

/// Shared mutable load-file list, containing only cached directory entries (0..15).
///
/// The active child initializes its viewport and selection; only one load list
/// may run at a time. Every row uses the shared checksum-gated callback.
extern UiList gMcLoadFileList;

/// Shared mutable save-destination list, containing files and an optional New Block row.
///
/// The parent supplies at most fifteen rows and the remembered directory index.
/// Only one save list may run at a time; every row uses the shared save callback.
extern UiList gMcSaveFileList;

/// Usable 8 KB blocks on a card, and the directory entries read for them.
enum {
    MEMORY_CARD_BLOCK_BYTES        = 0x2000, // One block: 64 sectors of 128 bytes
    MEMORY_CARD_SECTORS_PER_BLOCK  = 64,
    MEMORY_CARD_BLOCK_COUNT        = 15,
    MEMORY_CARD_DIRECTORY_CAPACITY = 15
};

/// `McWork.blockOwners` sentinels. A non-negative value is a directory index.
enum {
    MEMORY_CARD_BLOCK_FREE    = -1, // No file
    MEMORY_CARD_BLOCK_FOREIGN = -2  // Occupied by another product's file
};

/// `McWork.promptTimer` opens at the lead-in and a closing prompt ends below the limit.
enum {
    MEMORY_CARD_PROMPT_LEAD_FRAMES   = 0x10,
    MEMORY_CARD_PROMPT_DISMISS_LIMIT = -0x10
};

/// Values armed in `McWork.cardTimer`, and the refusal count that aborts a transfer.
enum {
    MEMORY_CARD_IO_BRIEF_FRAMES  = 4,
    MEMORY_CARD_IO_SETTLE_FRAMES = 0xE,
    MEMORY_CARD_IO_ABORT_FRAMES  = 0xB5
};

/// Number of save-file sections, including the title, palette and icon header.
enum { MEMORY_CARD_BUFFER_SLOT_COUNT = 9 };

/// All-bits `McWork.slotWriteMask`: transfer every slot. The directory listing then starts at slot 0.
enum { MEMORY_CARD_SLOT_WRITE_ALL = -1 };

/// `McWork.confirmOverwrite`.
enum {
    MEMORY_CARD_OVERWRITE_PROCEED = 0, // Keep the name and start saving
    MEMORY_CARD_OVERWRITE_CONFIRM = 1  // Ask with the overwrite prompt first
};

/// `McWork.corruptNoticeStyle`.
enum {
    MEMORY_CARD_CORRUPT_NOTICE_PROMPT    = 0, // One gray prompt line
    MEMORY_CARD_CORRUPT_NOTICE_MULTILINE = 1  // Several lines
};

/// Row of `_gMcPrompts` stored in `McWork.promptId`.
enum {
    MEMORY_CARD_PROMPT_ACCESS_FAILED = 0,   // Access failed; please try again
    MEMORY_CARD_PROMPT_CHECKING      = 1,   // Checking the card; do not remove it
    MEMORY_CARD_PROMPT_CHANGED       = 2,   // The card was changed
    MEMORY_CARD_PROMPT_NO_CARD       = 3,   // No card; insert one
    MEMORY_CARD_PROMPT_SAVING        = 4,   // Saving; do not remove the card
    MEMORY_CARD_PROMPT_LOADING       = 5,   // Loading; do not remove the card
    MEMORY_CARD_PROMPT_FORMATTING    = 6,   // Formatting; do not remove the card
    MEMORY_CARD_PROMPT_SAVE          = 7,   // Confirm saving
    MEMORY_CARD_PROMPT_LOAD          = 8,   // Confirm loading
    MEMORY_CARD_PROMPT_UNFORMATTED   = 9,   // Card is not formatted
    MEMORY_CARD_PROMPT_NO_DATA       = 0xA, // No game data on the card
    MEMORY_CARD_PROMPT_CREATE        = 0xB, // Confirm creating data
    MEMORY_CARD_PROMPT_CREATING      = 0xC, // Creating data; do not remove the card
    MEMORY_CARD_PROMPT_FORMAT_FAILED = 0xD,
    MEMORY_CARD_PROMPT_SAVE_FAILED   = 0xE,
    MEMORY_CARD_PROMPT_LOAD_FAILED   = 0xF,
    MEMORY_CARD_PROMPT_OVERWRITE     = 0x11, // Confirm replacing the selected file
    MEMORY_CARD_PROMPT_CARD_FULL     = 0x13,
    MEMORY_CARD_PROMPT_SELECT        = 0x16, // Choose a file
    MEMORY_CARD_PROMPT_CORRUPTED     = 0x17  // Save data is corrupted
};

/// Work area of the memory-card dialogs.
///
/// The save machine and the file-select machine share one of these with the
/// slot-list UI they spawn. It holds the card channel, the prompt and its
/// timers, the `MemCardSync` result, the transfer buffer, the directory and
/// each file's preview, and the map from card blocks to those files.
typedef struct {
    s32             promptTimer;                               // Lead-in, then dismiss count. Opens at 16 and steps toward 0 by 2; a closing prompt counts down by 1 and ends below -16
    s32             cardTimer;                                 // Frames until the next card call, or frames the current call has been refused. A wait fires at 0; either dispatcher aborts at 181
    s32             promptId;                                  // Row of `_gMcPrompts` drawn for this state
    s32             channel;                                   // `MemCard` channel. Port 0 is the only value stored
    long            syncCommand;                               // Command code reported by `MemCardSync`
    long            syncResult;                                // Result code reported by `MemCardSync`
    void*           buffer;                                    // Heap block for the current card transfer, or null
    s32             sectorOffset;                              // Next transfer position, in 128-byte card sectors
    s32             transferBytes;                             // Byte length of that transfer, rounded up to a sector
    s32             slotsRemaining;                            // `_gMcSaveSections` not yet visited, counting down from 9. The slot in hand is index `9 - slotsRemaining`; 0 means the walk is finished
    s32             slotWriteMask;                             // Bit n transfers buffer slot n. Shifted right unsigned as the walk advances, so bit 0 is the slot in hand. All bits set also makes the directory listing start at slot 0
    s32             confirmOverwrite;                          // 0 keep the name and start saving, 1 ask with the overwrite prompt first
    struct DIRENTRY directory[MEMORY_CARD_DIRECTORY_CAPACITY]; // Entries from the latest `MemCardGetDirentry`
    long            entryCount;                                // Entries written into `directory`
    s32             foreignBlockCount;                         // After the wildcard scan, blocks spanned by every file. The product listing subtracts this game's file count, leaving blocks held by other products; 15 means none remain
    s32             selectedSlot;                              // Directory index highlighted in the load list
    McSavePreview   previews[MEMORY_CARD_DIRECTORY_CAPACITY];  // First 128 bytes of each file's save
    s32             currentSlot;                               // Directory index whose preview is being read
    s32             closeAnswer;                               // Yes after the save walk or the saving prompt finishes; no from reset. Published as the panel result value when the prompt closes
    u16             checksum;                                  // Signed byte sum of the 512-byte transfer buffer, compared with the save's title checksum
    u16             checksumComplement;                        // Ones' complement of `checksum`, written in the same store as the sum
    s32             corruptNoticeStyle;                        // Corrupted-save notice (0 one gray prompt line, 1 several lines). The save machine writes 0; file select writes 1
    s8              blockOwners[0x10];                         // Block to directory index (-1 free, -2 another product, otherwise this game's file). The scan writes the first fifteen; what the last byte is for is unproven
} McWork;
STATIC_ASSERT_SIZEOF(McWork, 0xA34);

/// NUL-terminated label for the memory-card Yes choice.
extern u8 gMcTextYes[sizeof("Yes")];

/// NUL-terminated label for the memory-card No choice.
extern u8 gMcTextNo[sizeof("No")];

/// NUL-terminated label for the memory-card Cancel choice.
extern u8 gMcTextCancel[sizeof("Cancel")];

/// NUL-terminated label for the memory-card OK acknowledgment.
extern u8 gMcTextOk[sizeof("OK")];

// Save-slot detail labels and indexed descriptions.
extern u8* Mc_LocationTitleLabels[];

/// NUL-terminated TIME caption for a cached save preview's elapsed play time.
extern const u8 gMcPreviewTimeLabel[sizeof("TIME")];

/// NUL-terminated CLEAR caption shown beside a cached preview's completed-run count.
extern const u8 gMcPreviewClearLabel[sizeof("CLEAR")];

/// NUL-terminated space and opening parenthesis preceding the preview's save number.
extern const u8 gMcPreviewSaveNumberPrefix[sizeof(" (")];

/// NUL-terminated EXP caption for the cached preview's player experience.
extern const u8 gMcPreviewExpLabel[sizeof("EXP")];

/// NUL-terminated unavailable-stat text for opening saves without EXP or BP values.
extern const u8 gMcPreviewUnavailableValue[sizeof("---")];

/// NUL-terminated BP caption for the cached preview's bounty points.
extern const u8 gMcPreviewBpLabel[sizeof("BP")];

/// Save-preview label slots, including the retained Where am I? text at index zero.
enum { MEMORY_CARD_SAVE_POINT_PREVIEW_LABEL_COUNT = 17 };

/// NUL-terminated ASCII place labels indexed by the saved save-point byte.
///
/// Validated previews select 1..16; index 15 is Opening. Slot zero holds
/// Where am I? and is rejected by preview validation. Resident consumers
/// borrow the text without modifying or retaining ownership of it.
extern const u8* gMcSavePointPreviewLabels[MEMORY_CARD_SAVE_POINT_PREVIEW_LABEL_COUNT];

/// NUL-terminated Nightmare label for a completed preview with gameMode 3.
extern const u8 gMcPreviewNightmareLabel[sizeof("Nightmare")];

/// NUL-terminated Scavenger label for a completed preview with gameMode 2.
extern const u8 gMcPreviewScavengerLabel[sizeof("Scavenger")];

/// NUL-terminated Bounty label for a completed preview with gameMode 1.
extern const u8 gMcPreviewBountyLabel[sizeof("Bounty")];

/// NUL-terminated Replay label for a completed preview with gameMode 0.
extern const u8 gMcPreviewReplayLabel[sizeof("Replay")];

/// Draw a card file's cached preview, corrupt notice or New Block placeholder.
///
/// Borrows a live panel object and its dialog work. `directoryIndex` must be
/// nonnegative: values below `entryCount` select its cached preview (0..14),
/// while a free-block row at `entryCount` draws New Block. Origins are integer
/// pixel offsets relative to the panel content origin and must fit the signed
/// text-coordinate range after layout additions. Existing previews require
/// `gameMode` in 0..3; the header check validates only the place index and sum.
/// Opening previews omit the run mode and show unavailable EXP/BP. All scratch
/// requests and strings are consumed during the call.
void mcDrawFilePreview(struct UiObject* panelObject, const McWork* work, s32 directoryIndex, s32 originX, s32 originY);

/// Run one save-dialog state and enforce the card I/O wait limit.
///
/// An active task borrows a live `UiObject` through `spawnArg2.pointer` and
/// uses state indices 0..43, without a bounds check. Dismissal stores -1;
/// any negative state only checks for a nonzero teardown countdown.
/// The dialogs share resident work and must run one at a time, starting at
/// state zero with no outstanding transfer allocation or card request.
/// At the I/O wait limit, frees a non-null transfer and enters access failure
/// without waiting for SDK completion. Every nonnegative run also advances
/// the random sequence used for new card filenames.
void mcSaveDialogTask(Task* dialogTask);

/// Initialize and start card I/O, then reset the resident save records and options.
///
/// The SDK's automatic card control routine is disabled; dialog tasks submit
/// and poll card operations themselves.
void mcInit(void);

/// Run one load-dialog state, including file selection, and enforce the card I/O wait limit.
///
/// The task borrows a live `UiObject` through `spawnArg2.pointer`; its state
/// must be in 0..25, without a bounds check or negative dismissal sentinel.
/// The dialogs share resident work and must run one at a time, starting at
/// state zero with no outstanding transfer allocation or card request.
/// At the I/O wait limit, enters load failure without releasing a transfer
/// buffer or waiting for SDK completion. Failure dismissal also retains it.
void mcLoadDialogTask(Task* dialogTask);

#endif // MAIN_PRIVATE_MC_H
