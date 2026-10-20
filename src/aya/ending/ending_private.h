#ifndef SRC_AYA_ENDING_ENDING_PRIVATE_H
#define SRC_AYA_ENDING_ENDING_PRIVATE_H

#include <psyq/sys/types.h>

#include "common.h"

#include "aya/ending.h"

#include "gameplay/inventory.h"
#include "gameplay/items.h"

#include "main/mc.h"
#include "main/task_types.h"
#include "main/ui_types.h"

#include "rooms/shop_tier.h"

/// One credits picture to decode: the MDEC bitstream to read and the VRAM
/// rectangle its pixels go to.
///
/// The credits task allocates one record and refills it for each picture. It
/// is the spawn argument of the task that decodes the picture.
typedef struct {
    u16* vlcTable;      // Huffman table built by `DecDCTvlcBuild`, shared by every picture
    u16  resourceIndex; // Resource slot holding the picture's bitstream
    u16  field_6;       // Never accessed; role unproven
    u16  vramX;         // Left edge of the destination in VRAM
    u16  vramY;         // Top edge of the destination in VRAM
    u16  width;         // Picture width in pixels; decoding goes in 16-pixel-wide strips
    u16  height;        // Picture height in pixels
} EndingPictureDecode;
STATIC_ASSERT_SIZEOF(EndingPictureDecode, 0x10);

/// Operations of a credits row, the values of `EndingStfCommand::op`.
///
/// A row is drawn left to right from its own state: it starts centred on
/// column 0 with palette 7, and a column or palette command applies to what
/// follows it in that row only. Any other value is skipped. The dispatch has
/// a slot of its own for 9 that does nothing; the role of that value is
/// unproven.
enum {
    ENDING_STF_COMMAND_GLYPH         = 0,    // Draw font cell `arg`; consecutive glyphs are placed as one run
    ENDING_STF_COMMAND_PALETTE       = 3,    // Draw the glyphs after it with font palette `arg` (0 to 7)
    ENDING_STF_COMMAND_COLUMN_CENTER = 4,    // Centre what follows on layout column `arg`
    ENDING_STF_COMMAND_COLUMN_LEFT   = 5,    // Start what follows at layout column `arg`'s left edge
    ENDING_STF_COMMAND_COLUMN_RIGHT  = 6,    // End what follows at layout column `arg`'s right edge
    ENDING_STF_COMMAND_PICTURE       = 7,    // Show credits picture `arg`, the bitstream in resource slot `arg + 1`, at the selected anchor
    ENDING_STF_COMMAND_SPRITE        = 8,    // Draw image `arg` of the sprite table
    ENDING_STF_COMMAND_END           = 0xFF, // Ends the row
};

/// What a picture command's `arg` becomes once the command has run.
///
/// A row is drawn on every frame it is visible, but its picture has to start
/// decoding only once, so the command marks itself in the loaded file.
enum {
    ENDING_STF_PICTURE_STARTED = 0xFF,
};

/// One command of a credits row: an operation and its operand.
///
/// A row is an array of these ended by `ENDING_STF_COMMAND_END`.
typedef struct {
    u8 op;  // One of `ENDING_STF_COMMAND_*`
    u8 arg; // Index of the glyph, palette, column, picture or sprite that `op` names
} EndingStfCommand;
STATIC_ASSERT_SIZEOF(EndingStfCommand, 0x2);

/// Packing of `EndingStfGlyph::heightAndPage`.
enum {
    ENDING_STF_GLYPH_HEIGHT_MASK = 0x7F, // Cell height in pixels
    ENDING_STF_GLYPH_PAGE_SHIFT  = 7,    // Bit 7 is set for a cell on the second font page
};

/// One cell of the credits font: where the glyph sits in its texture page, and
/// its size.
typedef struct {
    u8 u;             // Left edge of the cell in its texture page
    u8 v;             // Top edge of the cell in its texture page
    u8 width;         // Cell width in pixels, which is also the pen advance
    u8 heightAndPage; // Height in bits 0-6; bit 7 selects the second font page
} EndingStfGlyph;
STATIC_ASSERT_SIZEOF(EndingStfGlyph, 0x4);

/// One image a credits row can place: its texture page, palette and rectangle.
///
/// An image that runs past the right edge of its texture page continues on the
/// page beside it.
typedef struct {
    u16 tpageX;    // Texture page X in VRAM
    u16 tpageY;    // Texture page Y in VRAM
    u16 clutX;     // Palette X in VRAM
    u16 clutY;     // Palette Y in VRAM
    u8  u;         // Left edge of the image in its first texture page
    u8  v;         // Top edge of the image in its texture page
    u8  pixelMode; // Texture format (0 4-bit, 1 8-bit, 2 15-bit)
    u16 width;     // Image width in pixels
    u16 height;    // Image height in pixels
} EndingStfSprite;
STATIC_ASSERT_SIZEOF(EndingStfSprite, 0x10);

/// Anchors of one layout column, in pixels from the left edge of the
/// 640-pixel-wide credits screen.
///
/// A row command picks a column together with the anchor that the text,
/// picture or image after it hangs from.
typedef struct {
    u16 centerX; // Centre of a centred run
    u16 leftX;   // Left edge of a left-aligned run
    u16 rightX;  // Right edge of a right-aligned run
    u16 field_6; // Never read; role unproven
} EndingStfColumn;
STATIC_ASSERT_SIZEOF(EndingStfColumn, 0x8);

enum {
    ENDING_STF_COLUMN_COUNT     = 8, // Layout columns in `EndingStfParams`
    ENDING_STF_HOLD_UNIT_FRAMES = 6, // Frames in one unit of the credits' hold times
};

/// Presentation parameters of the credits: scroll timing and layout columns.
typedef struct {
    u16             scrollSpeed;                      // Pixels scrolled per frame, 8.8 fixed point
    u8              startHold;                        // Wait before the scroll starts, in `ENDING_STF_HOLD_UNIT_FRAMES`
    u8              endHold;                          // Wait on the last rows once the scroll stops, same unit
    byte            field_4[0x20];                    // Never read; role unproven
    EndingStfColumn columns[ENDING_STF_COLUMN_COUNT]; // Indexed by a row command's column argument
} EndingStfParams;
STATIC_ASSERT_SIZEOF(EndingStfParams, 0x64);

/// One row of the credits: the commands that draw it and where it sits in the
/// scrolling document.
typedef struct {
    union {
        s32               offset;  // From the start of the file, as stored on disc
        EndingStfCommand* pointer; // Once the file is relocated
    } cmds;                        // Commands of the row, ended by `ENDING_STF_COMMAND_END`
    s32 y;                         // Bottom edge of the row, in pixels from the top of the document
} EndingStfLine;
STATIC_ASSERT_SIZEOF(EndingStfLine, 0x8);

/// The rows of the credits document: a count, then that many rows in the order
/// they scroll past, top first.
typedef struct {
    s32           count;    // Number of rows in `lines`
    EndingStfLine lines[0]; // The rows, by increasing `y`; the last one's `y` is the document's height
} EndingStfLineTable;
STATIC_ASSERT_SIZEOF(EndingStfLineTable, 0x4);

/// Header of an "STF" credits file: the four tables the credits are drawn
/// from.
///
/// On disc each table is a byte offset from the start of the file. Relocation
/// replaces the offsets with pointers in place, and does so once: a pointer
/// into RAM is negative as a signed word, so a file whose `glyphs` word is not
/// positive has already been relocated.
typedef struct {
    char magic[4]; // "STF" then a format digit; only the three letters are checked
    s32  field_4;  // Never read; role unproven
    union {
        s32              offset;
        EndingStfParams* pointer;
    } params; // Scroll timing and layout columns
    union {
        s32             offset;
        EndingStfGlyph* pointer;
    } glyphs; // Font cells, indexed by a glyph command's argument
    union {
        s32                 offset;
        EndingStfLineTable* pointer;
    } lineTable; // Rows of the document, top to bottom
    union {
        s32              offset;
        EndingStfSprite* pointer;
    } sprites; // Images, indexed by an image command's argument
} EndingStfFile;
STATIC_ASSERT_SIZEOF(EndingStfFile, 0x18);

/// What a completed game carries into the next one.
///
/// The Complete Bonus list works these out when it opens. The panels after it
/// show them, and building the next game's save applies them.
typedef struct {
    s32 totalExp;     // EXP earned this game: the balance plus what the learned upgrade levels cost
    s32 totalBp;      // BP balance plus the bonus BP of every listed item, capped at 99999999
    s32 nextExp;      // EXP the next game starts with: the balance times the mode's multiplier, capped at 9999999
    s32 nextBp;       // BP the next game starts with, before `extraBonusBp`: `totalBp` times the multiplier, capped at 9999999
    s32 shopTier;     // Shop tier this clear unlocks, or -1 when every tier is already unlocked
    s32 extraBonusBp; // BP granted in place of a tier when none is left to unlock, otherwise 0
} EndingTotals;
STATIC_ASSERT_SIZEOF(EndingTotals, 0x18);

/// Number of eligible catalog ids, excluding the two stored trailer halfwords.
enum { ENDING_ITEM_WHITELIST_COUNT = 78 };

/// Catalog ids eligible for the Complete Bonus item-credit list.
///
/// The first 78 entries are searched by count: 70 ordinary item ids and eight
/// collection ids. A 0xFFFF terminator and an unused zero follow them. Saved
/// non-consumable rows contribute once per row; collection flags contribute
/// once per eligible id. Storage is read-only for the loaded package's lifetime.
extern const u16 gEndingItemWhitelist[ENDING_ITEM_WHITELIST_COUNT + 2];

/// Auxiliary-heap RGB16 pixels for two alternating 16-pixel-wide picture strips.
///
/// Each strip occupies `gEndingPictureHeight * 8` 32-bit words. MDEC fills
/// the selected strip, then its completion callback submits it to VRAM before
/// toggling the buffer index. The serialized decoder owns the allocation until
/// its final upload; the retained pointer is invalid after it is released.
extern u_long* gEndingPictureStripPixels;

/// Auxiliary-heap expanded DCT bitstream for the current credits picture.
///
/// VLC expansion writes this word-aligned allocation before MDEC reads it.
/// Capacity is width * height * 2 bytes; the loaded bitstream must fit it.
/// The serialized decoder releases it after the final strip upload without
/// clearing this retained pointer.
extern u_long* gEndingPictureDctBitstream;

/// Current decoded picture's width in pixels, copied from the decode request.
///
/// Valid requests use signed-halfword dimensions, width at least 32 and a
/// multiple of 16. Completion occurs after width / 16 - 1 uploaded strips;
/// the final 16-pixel column is left at its pre-cleared colour.
extern s16 gEndingPictureWidth;

/// Current decoded picture's height in pixels, positive and a multiple of 16.
///
/// Copied from the request and interpreted as a signed halfword for allocation,
/// MDEC output lengths and each strip's VRAM rectangle. Requests must fit 32767.
extern s16 gEndingPictureHeight;

/// Current picture's destination X in VRAM words, copied from the request.
///
/// RGB16 uses one word per pixel; strip n starts at X + n * 16. The complete
/// destination rectangle must fit VRAM while the serialized decoder runs.
extern u16 gEndingPictureVramX;

/// Current picture's destination Y in VRAM rows, copied from the request.
///
/// Every uploaded strip uses this Y and the full picture height.
extern u16 gEndingPictureVramY;

/// Values of the serialized decoder's strip-output handshake.
enum {
    ENDING_PICTURE_STRIP_UPLOADED = 0,
    ENDING_PICTURE_STRIP_PENDING  = 1,
};

/// MDEC strip-output handshake (0 upload submitted, 1 output pending).
///
/// The task queues output only after the completion callback clears this flag.
/// Requests are serialized; this flag describes the selected strip buffer.
extern s16 gEndingPictureStripPending;

/// Number of picture strips submitted to VRAM, also the next strip's X index.
///
/// Starts at zero, advances after each 16-pixel upload and stops at width / 16
/// minus one. The callback retains unsigned-halfword increment and signed
/// completion comparisons; valid requests keep the count nonnegative.
extern s16 gEndingPictureStripsUploaded;

/// Selected RAM strip buffer (0 first, 1 second), toggled after each upload.
///
/// Reset for each decode; each half contains height * 8 32-bit words.
extern u16 gEndingPictureStripBufferIndex;

/// Thirteen EXP ceilings and item triples used to choose the next shop unlock.
///
/// Indexed by saved tier bits. Selection applies the game-mode boost and skips
/// unlocked rows; a fully unlocked ladder grants its rounded purchase-price
/// total as extra BP instead. This package owns a read-only copy independent
/// of the equal tables in shop room images.
extern const ShopTier gEndingShopTierLadder[SHOP_TIER_COUNT];

/// NUL-terminated three-line warning before discarding unsaved game-clear data.
extern const u8 gEndingQuitWarningText[];

/// NUL-terminated warning line used as the quit panel's horizontal width sample.
///
/// The complete warning is drawn separately; this is only the width sample.
extern const u8 gEndingQuitWarningWidthText[];

/// Bodyless priority-192 recipe for the NEXT REPLAY BONUS child balance panel.
///
/// Initial bounds are centred-screen pixels and ordering-table base is tag 52.
/// The Balance panel spawns it with argument 1 equal to 1 and itself as parent.
/// Spawning copies the recipe; replay and gameplay must remain loaded through
/// the child tree's closure.
extern const UiObjectDesc gEndingNextEndingPanelDescriptor;

/// Completed-run totals and next-replay awards shared by the award panels.
///
/// Complete Bonus computes these before presentation; cleared-save preparation
/// later applies them. Only one award sequence may use this mutable record.
extern EndingTotals gEndingTotals;

/// Mutable Complete Bonus list, shared with its item-credit row callback.
///
/// Initialized with one row and 15-pixel spacing; the panel fills its item count,
/// fits nine visible rows and enables the single shared callback. The owner
/// task holds the item-id storage. Only one panel may use it at a time, and
/// reachable indices and visible counts must fit nonnegative signed bytes.
extern UiList gEndingCompleteBonusList;

/// Returns an item's replay-clear BP credit: half its unsigned purchase price, rounded down.
///
/// Requires the gameplay catalog to remain loaded. Declared catalog ids
/// are 0..0xBF and 0x100..0x17F; no bounds check is performed. The Complete
/// Bonus list supplies only its whitelisted ordinary and collection ids.
/// The result is 0..32767 BP and neither prices nor inventory are changed.
static inline s32 _endingItemBp(s32 itemId)
{
    enum { ENDING_ITEM_CREDIT_SHIFT = 1 };
    s32 purchasePriceBp;

    if (itemId < ITEM_TEXT_KEY_ID_FIRST) {
        purchasePriceBp = gItemDescriptors[itemId].price;
    } else {
        purchasePriceBp = gItemKeyDescriptors[itemId - ITEM_TEXT_KEY_ID_FIRST].price;
    }
    purchasePriceBp >>= ENDING_ITEM_CREDIT_SHIFT;
    return purchasePriceBp;
}

/// Allocates and builds the credits pictures' VLC table on the auxiliary heap.
///
/// The caller owns the table and must keep that heap intact until decoding
/// ends, then release it with `memFreeFromHeap(table, true)`. Allocation
/// failure is not handled before the SDK builds the table.
u16* endingCreatePictureVlcTable(void);

/// Returns the completed run's EXP balance plus the cost of every learned level.
///
/// Reads the first twelve saved ability levels (each 0..3). Discounts apply
/// separately to each level, using this run's mode and previous clear count.
/// The signed result is an EXP amount, also compared with unsigned shop ceilings.
s32 endingGetTotalExp(void);

/// Returns the shop tier this clear can unlock, or -1 when all thirteen are unlocked.
///
/// Requires the completed run's live save, mode 0..3, and a thirteen-bit
/// unlock mask. Starts at the first inclusive ceiling of total earned EXP,
/// adds the mode, caps at the last tier, then searches cyclically past
/// unlocked rows. EXP is compared as unsigned; a value beyond every ceiling
/// retains starting tier zero. Does not change the save or grant items.
static inline s32 _endingResolveShopTier(void)
{
    enum { ENDING_SHOP_TIER_NONE = -1 };
    const ShopTier*   tierRow;
    u32               totalExp;
    s32               tierIndex;
    s32               tiersChecked;
    const McSaveData* saveData;
    s32               unlockedTiers;
    s32               tierBit;
    s32               selectedTier;

    totalExp  = endingGetTotalExp();
    tierRow   = gEndingShopTierLadder;
    tierIndex = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers == SHOP_TIER_ALL_MASK) {
        selectedTier = ENDING_SHOP_TIER_NONE;
    } else {
        // Apply the EXP ceiling and mode boost before searching for an unlock.
        for (tiersChecked = 0; tiersChecked < SHOP_TIER_COUNT; tiersChecked++, tierRow++) {
            if (tierRow->expCeiling >= totalExp) {
                tierIndex = tiersChecked;
                break;
            }
        }

        saveData     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        tierIndex   += saveData->state.gameMode;
        tiersChecked = 0;
        if (tierIndex >= SHOP_TIER_COUNT) {
            tierIndex = SHOP_TIER_COUNT - 1;
        }
        // Skip unlocked tiers cyclically.
        tierBit       = 1;
        unlockedTiers = saveData->state.shopTiers;
        for (; tiersChecked < SHOP_TIER_COUNT; tiersChecked++) {
            if ((unlockedTiers & (tierBit << tierIndex)) == 0) {
                break;
            }
            tierIndex += 1;
            if (tierIndex >= SHOP_TIER_COUNT) {
                tierIndex -= SHOP_TIER_COUNT;
            }
        }
        selectedTier = tierIndex;
    }
    return selectedTier;
}

/// Replaces the live save with a cleared-game save carrying the replay awards.
///
/// Requires computed replay totals and the completed run's live save and player
/// status. Preserves options, identification and usage history, raises records
/// and shop unlocks, and sets the next game's EXP/BP in both save and player.
/// Run progress and inventory are reset; this does not write the memory card.
void endingPrepareClearedSave(void);

/// Decodes one borrowed credits picture into VRAM using auxiliary-heap buffers.
///
/// `task->spawnArg2.pointer` is a `EndingPictureDecode` whose record, VLC
/// table and resource bitstream remain live until the task ends. Dimensions
/// must fit signed halfwords: width is at least 32, height is positive, and
/// both are multiples of 16 pixels. The destination must fit VRAM.
/// `resourceIndex` is 0..49; its expanded bitstream must fit
/// `width * height * 2` bytes. Requests are serialized because the buffers
/// and MDEC completion callback are shared. Releases its two scratch buffers
/// on normal completion, retaining the caller's picture and VLC table.
void endingDecodePictureTask(Task* task);

/// Computes replay awards and scrolls the completed run's Complete Bonus item list.
///
/// Starts at state zero with a live UI object in `task->spawnArg2.pointer`.
/// Allocates 300 signed-halfword item ids on the primary heap; task teardown
/// releases them. Keeps the live save, gameplay catalog and replay package
/// loaded, with only one Complete Bonus panel using the shared list/totals.
/// Navigation requires at most 127 emitted rows; legal-save limits remain
/// unproven. Holds the last rows for 60 callbacks, scrolls toward the first,
/// then holds for 188 callbacks. Reports confirm after the final hold or
/// active Cancel/Menu; allocation failure confirms immediately. Items are
/// identified while drawn, and EXP/BP and shop unlocks are applied later
/// when preparing the cleared save.
void endingCompleteBonusPanelTask(Task* task);

/// Presents completed-run balances or the base next-replay awards.
///
/// `task->spawnArg2.pointer` is its live UI object. Spawn argument 1 is 0 for
/// Balance, which opens a child panel after the hold, or 1 for NEXT REPLAY
/// BONUS, which confirms after the hold. Cancellation and child confirmation
/// also confirm the parent. Balance shows EXP in hand and BP including item
/// credit; NEXT REPLAY BONUS shows mode-scaled awards before extra BP.
void endingBalancePanelTask(Task* task);

/// Asks whether to quit without saving the cleared-game data, defaulting to No.
///
/// `task->spawnArg2.pointer` is its live UI object. Child Yes/No commands are
/// returned in `resultValue` with a confirm result for the screen controller.
void endingQuitWarningTask(Task* task);

/// Shows one item of the shop tier unlocked by this clear, then confirms.
///
/// `task->spawnArg2.pointer` is its live UI object and spawn argument 1 starts
/// as the item column (0..2). Reuses that argument for `itemMenuInfoTask`'s
/// item id and next-replay flag, retaining the column in `extraState`.
/// Confirms immediately when no tier remains; does not grant inventory items.
void endingUnlockedItemPanelTask(Task* task);

/// Shows the extra BP awarded when every shop tier is already unlocked.
///
/// `task->spawnArg2.pointer` is its live UI object. Confirms when the active
/// panel's hold expires or Cancel/Menu is pressed; currency is applied later.
void endingExtraBpPanelTask(Task* task);

#endif // SRC_AYA_ENDING_ENDING_PRIVATE_H
