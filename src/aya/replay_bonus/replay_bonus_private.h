#ifndef SRC_AYA_REPLAY_BONUS_REPLAY_BONUS_PRIVATE_H
#define SRC_AYA_REPLAY_BONUS_REPLAY_BONUS_PRIVATE_H

#include <psyq/sys/types.h>

#include "common.h"

#include "gameplay/inventory.h"
#include "gameplay/items.h"

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
} ReplayBonusPictureDecode;
STATIC_ASSERT_SIZEOF(ReplayBonusPictureDecode, 0x10);

/// Operations of a credits row, the values of `ReplayBonusStfCommand::op`.
///
/// A row is drawn left to right from its own state: it starts centred on
/// column 0 with palette 7, and a column or palette command applies to what
/// follows it in that row only. Any other value is skipped. The dispatch has
/// a slot of its own for 9 that does nothing; the role of that value is
/// unproven.
enum {
    REPLAY_BONUS_STF_COMMAND_GLYPH         = 0,    // Draw font cell `arg`; consecutive glyphs are placed as one run
    REPLAY_BONUS_STF_COMMAND_PALETTE       = 3,    // Draw the glyphs after it with font palette `arg` (0 to 7)
    REPLAY_BONUS_STF_COMMAND_COLUMN_CENTER = 4,    // Centre what follows on layout column `arg`
    REPLAY_BONUS_STF_COMMAND_COLUMN_LEFT   = 5,    // Start what follows at layout column `arg`'s left edge
    REPLAY_BONUS_STF_COMMAND_COLUMN_RIGHT  = 6,    // End what follows at layout column `arg`'s right edge
    REPLAY_BONUS_STF_COMMAND_PICTURE       = 7,    // Show credits picture `arg`, the bitstream in resource slot `arg + 1`, at the selected anchor
    REPLAY_BONUS_STF_COMMAND_SPRITE        = 8,    // Draw image `arg` of the sprite table
    REPLAY_BONUS_STF_COMMAND_END           = 0xFF, // Ends the row
};

/// What a picture command's `arg` becomes once the command has run.
///
/// A row is drawn on every frame it is visible, but its picture has to start
/// decoding only once, so the command marks itself in the loaded file.
enum {
    REPLAY_BONUS_STF_PICTURE_STARTED = 0xFF,
};

/// One command of a credits row: an operation and its operand.
///
/// A row is an array of these ended by `REPLAY_BONUS_STF_COMMAND_END`.
typedef struct {
    u8 op;  // One of `REPLAY_BONUS_STF_COMMAND_*`
    u8 arg; // Index of the glyph, palette, column, picture or sprite that `op` names
} ReplayBonusStfCommand;
STATIC_ASSERT_SIZEOF(ReplayBonusStfCommand, 0x2);

/// Packing of `ReplayBonusStfGlyph::heightAndPage`.
enum {
    REPLAY_BONUS_STF_GLYPH_HEIGHT_MASK = 0x7F, // Cell height in pixels
    REPLAY_BONUS_STF_GLYPH_PAGE_SHIFT  = 7,    // Bit 7 is set for a cell on the second font page
};

/// One cell of the credits font: where the glyph sits in its texture page, and
/// its size.
typedef struct {
    u8 u;             // Left edge of the cell in its texture page
    u8 v;             // Top edge of the cell in its texture page
    u8 width;         // Cell width in pixels, which is also the pen advance
    u8 heightAndPage; // Height in bits 0-6; bit 7 selects the second font page
} ReplayBonusStfGlyph;
STATIC_ASSERT_SIZEOF(ReplayBonusStfGlyph, 0x4);

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
} ReplayBonusStfSprite;
STATIC_ASSERT_SIZEOF(ReplayBonusStfSprite, 0x10);

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
} ReplayBonusStfColumn;
STATIC_ASSERT_SIZEOF(ReplayBonusStfColumn, 0x8);

enum {
    REPLAY_BONUS_STF_COLUMN_COUNT     = 8, // Layout columns in `ReplayBonusStfParams`
    REPLAY_BONUS_STF_HOLD_UNIT_FRAMES = 6, // Frames in one unit of the credits' hold times
};

/// Presentation parameters of the credits: scroll timing and layout columns.
typedef struct {
    u16                  scrollSpeed;                            // Pixels scrolled per frame, 8.8 fixed point
    u8                   startHold;                              // Wait before the scroll starts, in `REPLAY_BONUS_STF_HOLD_UNIT_FRAMES`
    u8                   endHold;                                // Wait on the last rows once the scroll stops, same unit
    byte                 field_4[0x20];                          // Never read; role unproven
    ReplayBonusStfColumn columns[REPLAY_BONUS_STF_COLUMN_COUNT]; // Indexed by a row command's column argument
} ReplayBonusStfParams;
STATIC_ASSERT_SIZEOF(ReplayBonusStfParams, 0x64);

/// One row of the credits: the commands that draw it and where it sits in the
/// scrolling document.
typedef struct {
    union {
        s32                    offset;  // From the start of the file, as stored on disc
        ReplayBonusStfCommand* pointer; // Once the file is relocated
    } cmds;                             // Commands of the row, ended by `REPLAY_BONUS_STF_COMMAND_END`
    s32 y;                              // Bottom edge of the row, in pixels from the top of the document
} ReplayBonusStfLine;
STATIC_ASSERT_SIZEOF(ReplayBonusStfLine, 0x8);

/// The rows of the credits document: a count, then that many rows in the order
/// they scroll past, top first.
typedef struct {
    s32                count;    // Number of rows in `lines`
    ReplayBonusStfLine lines[0]; // The rows, by increasing `y`; the last one's `y` is the document's height
} ReplayBonusStfLineTable;
STATIC_ASSERT_SIZEOF(ReplayBonusStfLineTable, 0x4);

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
        s32                   offset;
        ReplayBonusStfParams* pointer;
    } params; // Scroll timing and layout columns
    union {
        s32                  offset;
        ReplayBonusStfGlyph* pointer;
    } glyphs; // Font cells, indexed by a glyph command's argument
    union {
        s32                      offset;
        ReplayBonusStfLineTable* pointer;
    } lineTable; // Rows of the document, top to bottom
    union {
        s32                   offset;
        ReplayBonusStfSprite* pointer;
    } sprites; // Images, indexed by an image command's argument
} ReplayBonusStfFile;
STATIC_ASSERT_SIZEOF(ReplayBonusStfFile, 0x18);

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
} ReplayBonusTotals;
STATIC_ASSERT_SIZEOF(ReplayBonusTotals, 0x18);

/// Replay-bonus item-id table (`0x4E` ids, then a `0xFFFF` terminator).
/// `func_replay_bonus_80117598` tests membership; `_replayBonusBuildItemList`
/// walks the same list when filling the owner task's `s16` item-id array.
extern u16 D_replay_bonus_8011908C[];

/// Double-buffered MDEC strip pixels. `_replayBonusUploadPictureStrip` LoadImage's
/// one 16-pixel-wide column from `buf[(flip << 5) * (s16)height]`.
extern u8* D_replay_bonus_8011925C;

/// VLC-decoded MDEC bitstream; `DecDCTin` source and `DecDCTvlc2` dest.
extern u_long* D_replay_bonus_80119260;

/// Full image width in pixels; strip count is `width / 16`.
extern s16 D_replay_bonus_80119264;

/// Image height in pixels, also RECT.h of each uploaded strip.
extern u16 D_replay_bonus_80119266;

/// VRAM destination x of strip 0.
extern u16 D_replay_bonus_80119268;

/// VRAM destination y.
extern u16 D_replay_bonus_8011926A;

/// Nonzero while a strip decode is in flight; the out-callback clears it.
extern s16 D_replay_bonus_8011926C;

/// Current strip index; dest x is `vramX + strip * 16`.
extern s16 D_replay_bonus_8011926E;

/// 0/1 selector for the double-buffer; toggled after each LoadImage.
extern u16 D_replay_bonus_80119270;

extern ShopTier D_replay_bonus_80118F78[SHOP_TIER_COUNT];

/// TaskDesc for the MDEC stream worker (`replayBonusDecodePictureTask`).
extern TaskDesc D_replay_bonus_80118F6C;

/// Stream phase: 0 idle, 1 running, 2 finished. Spawn is skipped when
/// `(u32)(phase - 1) < 2` (already running or finished).
extern u8 D_replay_bonus_80119225;

/// 0/1 VRAM-Y flip; xor'd when a stream finishes.
extern u8 D_replay_bonus_80119226;

/// Post-stream wait (`0x78` frames) written when the worker is polled dead.
extern u8 D_replay_bonus_80119227;

/// The live `D_replay_bonus_80118F6C` worker, or NULL.
extern Task* D_replay_bonus_80119228;

/// Heap pointer to the current `ReplayBonusPictureDecode`.
extern ReplayBonusPictureDecode* D_replay_bonus_801192BC;

extern ReplayBonusStfFile* D_replay_bonus_8011928C;

/// TaskDesc table spawned from the credits task (hold / fade / stream workers).
extern TaskDesc D_replay_bonus_8011922C[4];

/// Relocated STF glyph and sprite tables.
extern ReplayBonusStfGlyph* D_replay_bonus_80119290;

extern ReplayBonusStfSprite* D_replay_bonus_8011929C;

/// Relocated STF presentation parameters.
extern ReplayBonusStfParams* D_replay_bonus_80119294;

/// Relocated STF row array; `D_replay_bonus_801192A0` is the count.
extern ReplayBonusStfLine* D_replay_bonus_80119298;

extern s32 D_replay_bonus_801192A0;

/// Integer scroll Y; starts at `-0x1E0`.
extern s32 D_replay_bonus_801192A4;

/// 8.8 fractional accumulator for the scroll.
extern s32 D_replay_bonus_801192A8;

/// Frame counter incremented while the credits draw.
extern s32 D_replay_bonus_801192B0;

/// Credits primitive-buffer selector and current buffer address.
extern u8 D_replay_bonus_80119224;

extern u8* D_replay_bonus_801192C0;

/// Bytes allocated in the current credits primitive buffer.
extern s32 D_replay_bonus_801192B4;

/// Stream sprite brightness and horizontal position.
extern u8 D_replay_bonus_801192AC;

extern u16 D_replay_bonus_801192B8;

extern ReplayBonusTotals D_replay_bonus_80119274;

extern UiList D_replay_bonus_80119130;

/// The BP an item is worth on the replay-bonus screen: half its descriptor
/// price, looked up in `Gp_ItemDescs` below id 0x100 and in `Gp_KeyItemDescs` above.
static inline s32 replayBonusItemBp(s32 id)
{
    s32 price;

    if (id < 0x100) {
        price = Gp_ItemDescs[id].price;
    } else {
        price = Gp_KeyItemDescs[id - 0x100].price;
    }
    price >>= 1;
    return price;
}

/// Allocates and builds the credits pictures' VLC table on the auxiliary heap.
///
/// The caller owns the table and must keep that heap intact until decoding
/// ends, then release it with `memFreeFromHeap(table, true)`. Allocation
/// failure is not handled before the SDK builds the table.
u16* replayBonusCreatePictureVlcTable(void);

/// Returns the completed run's EXP balance plus the cost of every learned level.
///
/// Reads the first twelve saved ability levels (each 0..3). Discounts apply
/// separately to each level, using this run's mode and previous clear count.
/// The signed result is an EXP amount, also compared with unsigned shop ceilings.
s32 replayBonusGetTotalExp(void);

/// Replaces the live save with a cleared-game save carrying the replay awards.
///
/// Requires computed replay totals and the completed run's live save and player
/// status. Preserves options, identification and usage history, raises records
/// and shop unlocks, and sets the next game's EXP/BP in both save and player.
/// Run progress and inventory are reset; this does not write the memory card.
void replayBonusPrepareClearedSave(void);

/// Decodes one borrowed credits picture into VRAM using auxiliary-heap buffers.
///
/// `task->spawnArg2.pointer` is a `ReplayBonusPictureDecode` whose record, VLC
/// table and resource bitstream remain live until the task ends. Dimensions
/// must fit signed halfwords: width is at least 32, height is positive, and
/// both are multiples of 16 pixels. The destination must fit VRAM.
/// `resourceIndex` is 0..49; its expanded bitstream must fit
/// `width * height * 2` bytes. Requests are serialized because the buffers
/// and MDEC completion callback are shared. Releases its two scratch buffers
/// on normal completion, retaining the caller's picture and VLC table.
void replayBonusDecodePictureTask(Task* task);

/// Presents completed-run balances or the base next-replay awards.
///
/// `task->spawnArg2.pointer` is its live UI object. Spawn argument 1 is 0 for
/// Balance, which opens a child panel after the hold, or 1 for NEXT REPLAY
/// BONUS, which confirms after the hold. Cancellation and child confirmation
/// also confirm the parent. Balance shows EXP in hand and BP including item
/// credit; NEXT REPLAY BONUS shows mode-scaled awards before extra BP.
void replayBonusBalancePanelTask(Task* task);

/// Asks whether to quit without saving the cleared-game data, defaulting to No.
///
/// `task->spawnArg2.pointer` is its live UI object. Child Yes/No commands are
/// returned in `resultValue` with a confirm result for the screen controller.
void replayBonusQuitWarningTask(Task* task);

/// Shows one item of the shop tier unlocked by this clear, then confirms.
///
/// `task->spawnArg2.pointer` is its live UI object and spawn argument 1 starts
/// as the item column (0..2). Reuses that argument for `itemMenuInfoTask`'s
/// item id and next-replay flag, retaining the column in `extraState`.
/// Confirms immediately when no tier remains; does not grant inventory items.
void replayBonusUnlockedItemPanelTask(Task* task);

/// Shows the extra BP awarded when every shop tier is already unlocked.
///
/// `task->spawnArg2.pointer` is its live UI object. Confirms when the active
/// panel's hold expires or Cancel/Menu is pressed; currency is applied later.
void replayBonusExtraBpPanelTask(Task* task);

#endif // SRC_AYA_REPLAY_BONUS_REPLAY_BONUS_PRIVATE_H
