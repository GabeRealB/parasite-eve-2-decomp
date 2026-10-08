#include "telephone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"
#include "types.h"

#include "gameplay/attachment_state.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

/// Units and catalogue limits of the telephone statistics panels.
enum {
    TELEPHONE_PERCENT_WHOLE             = 10000,
    TELEPHONE_PERCENT_FRACTIONAL_DIGITS = 2,
    TELEPHONE_PERCENT_UNITS_PER_PERCENT = 100,
    TELEPHONE_SUSPENDED_CONTROL_SHIFT   = 16,
    TELEPHONE_PANEL_INITIALIZE          = 0,
    TELEPHONE_GAUGE_FRACTION_BITS       = 12,
    TELEPHONE_USAGE_GAUGE_HEIGHT        = 9,
    TELEPHONE_MAX_UNSCALED_USE_COUNT    = 99999,
    TELEPHONE_PE_ITEM_FIRST             = 15,
    TELEPHONE_PE_SLOTS_PER_PAGE         = 3,
    TELEPHONE_EXTERMINATION_TOTAL       = 326,
    TELEPHONE_USAGE_WEAPONS             = 0,
    TELEPHONE_USAGE_PARASITE_ENERGY     = 1
};

/// Row indices dispatched by the nine-entry play-data list.
enum {
    TELEPHONE_PLAY_DATA_TIME,
    TELEPHONE_PLAY_DATA_SAVES,
    TELEPHONE_PLAY_DATA_WINS,
    TELEPHONE_PLAY_DATA_ESCAPES,
    TELEPHONE_PLAY_DATA_WIN_PERCENT,
    TELEPHONE_PLAY_DATA_EXTERMINATION_PERCENT,
    TELEPHONE_PLAY_DATA_CLEARS,
    TELEPHONE_PLAY_DATA_MAX_EXP,
    TELEPHONE_PLAY_DATA_MAX_BP,
    TELEPHONE_PLAY_DATA_ROW_COUNT
};

/// Telephone menu phases; statistics-panel dismissal returns to the list.
enum {
    TELEPHONE_MENU_INITIALIZE            = 0,
    TELEPHONE_MENU_WAIT_SAVE             = 1,
    TELEPHONE_MENU_WAIT_STATISTICS       = 2,
    TELEPHONE_MENU_WAIT_NOTICE_AND_CLOSE = 3
};

/// Rows a usage panel has room for. The weapon list can fill every one, since
/// the save keeps this many weapon use counters; the Parasite Energy list has
/// at most twelve.
#define TELEPHONE_USAGE_ROW_CAPACITY 0x20

/// Work block of the usage-panel task: the rows of the "Weapon Data" or
/// "PE Data" list, most used first.
///
/// The task allocates it zeroed on its first frame and parks it in
/// `Task::work`, so the task's default teardown frees it. One of the two
/// builders fills the first `UiList::itemCount` entries of each array, and the
/// list's row callback reads them by the row it is drawing. Both lists hold
/// item ids: a weapon row its weapon (0x80-0x9F), a Parasite Energy row the
/// level of that ability the player holds (three consecutive ids per ability,
/// from 0xF).
typedef struct {
    s16  itemIds[TELEPHONE_USAGE_ROW_CAPACITY];        // Item each row names, previews and opens in the detail window
    s16  usageShares[TELEPHONE_USAGE_ROW_CAPACITY];    // Row's share of all recorded uses, rounded, in hundredths of a percent (0..10000)
    s16  gaugeFractions[TELEPHONE_USAGE_ROW_CAPACITY]; // Row's use count over the top row's, as a 12-bit fraction (4096 fills the gauge)
    byte unknown_C0[4];                                // Allocated and zeroed, never accessed; role unproven
} _TelephoneUsageWork;
STATIC_ASSERT_SIZEOF(_TelephoneUsageWork, 0xC4);

static void _telephoneDrawUsageRow(UiList* list, UiObject* object);

static void _telephonePromptTaskExit(Task* task);

/// Shifts a digit string and its NUL to open a decimal-point slot.
///
/// All arguments are simple writable locals, evaluated repeatedly. The cursor
/// starts at the first digit, digitCount at zero, and trailingDigits positive.
/// The string needs one spare byte. digitCount and shiftIndex may be the same
/// local: the length is no longer needed when the byte-shifting loop begins.
#define TELEPHONE_SHIFT_DECIMAL_DIGITS(digitCursor, digitCount, trailingDigits, shiftIndex) \
    {                                                                                       \
        while (*(digitCursor) != 0) {                                                       \
            (digitCursor)++;                                                                \
            (digitCount)++;                                                                 \
        }                                                                                   \
        if ((digitCount) < (trailingDigits)) {                                              \
            (trailingDigits) = (digitCount);                                                \
        }                                                                                   \
        (trailingDigits)++;                                                                 \
        for ((shiftIndex) = 0; (shiftIndex) < (trailingDigits); (shiftIndex)++) {           \
            (digitCursor)[1] = (digitCursor)[0];                                            \
            (digitCursor)--;                                                                \
        }                                                                                   \
        (digitCursor)[1] = '.';                                                             \
    }

/// Draws the selected play-data statistic and supplies its row help text.
///
/// The shared callback receives a list row index in 0..8 and a live panel object.
/// Time is saved minutes; the two percentages use hundredths of a percent.
/// Extermination includes the two Neo Ark roamer pools, divides by 326, and adds
/// a separator and five pixels to the row pen. Values are right-aligned beside
/// their captions. The panel requires loaded text resources and writable GPU
/// storage; its 32-byte temporary buffer holds every formatted saved value.
static void _telephoneDrawPlayDataRow(UiList* list, UiObject* object)
{
    // Draw a medium-font caption at the row's baseline. Captures list/object
    // and each case's captionRequest/captionBaseY. The
    // caption expression is evaluated once; row and panel reads stay ordered.
#define TELEPHONE_DRAW_PLAY_DATA_CAPTION(caption)                                                              \
    {                                                                                                          \
        captionRequest.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue; \
        captionBaseY              = object->panel.contentOriginY.unsignedValue - 6;                            \
        captionRequest.y          = list->rowTextY.unsignedValue + captionBaseY;                               \
        captionRequest.otIndex    = object->panel.otIndex.signedValue + 1;                                     \
        captionRequest.colorRgb   = list->colorRgb;                                                            \
        captionRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;                                                   \
        captionRequest.alignment  = TEXT_ALIGNMENT_LEFT;                                                       \
        captionRequest.drawMode   = TEXT_DRAW_OUTLINED;                                                        \
        textDrawString(&captionRequest, (caption));                                                            \
    }

    u8  valueText[0x20];
    u8* formatBuffer;

    formatBuffer = valueText;
    if (((object->panel.control.word >> TELEPHONE_SUSPENDED_CONTROL_SHIFT) == USER_INTERFACE_PANEL_ACTIVE) || (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            u8* helpTextByRow[TELEPHONE_PLAY_DATA_ROW_COUNT] = {
                Telephone_Data_80181A7C,
                Telephone_Data_80181AA8,
                Telephone_Data_80181ACC,
                Telephone_Data_80181AFC,
                Telephone_Data_80181B30,
                Telephone_Data_80181B64,
                Telephone_Data_80181B9C,
                Telephone_Data_80181BD0,
                Telephone_Data_80181C08,
            };

            uiSetPromptText(helpTextByRow[list->currentItemIndex], 0, 0);
        }
    }

    // Percentages use the recorded battle totals, without display clamping.
    switch (list->currentItemIndex) {
        case TELEPHONE_PLAY_DATA_TIME: {
            TextDrawReq captionRequest;
            s32         captionBaseY;

            TELEPHONE_DRAW_PLAY_DATA_CAPTION(Telephone_Data_80181A20);
            textFormatPlayTime(formatBuffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime);
            textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, valueText, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case TELEPHONE_PLAY_DATA_SAVES: {
            TextDrawReq captionRequest;
            s32         captionBaseY;

            TELEPHONE_DRAW_PLAY_DATA_CAPTION(Telephone_Data_80181A50);
            textItoaUnsigned(formatBuffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount);
            textAppendString(formatBuffer, Telephone_Data_80181A70);
            textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, valueText, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case TELEPHONE_PLAY_DATA_WINS: {
            TextDrawReq captionRequest;
            s32         captionBaseY;

            TELEPHONE_DRAW_PLAY_DATA_CAPTION(Telephone_Data_80181A28);
            textItoaUnsigned(formatBuffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon);
            textAppendString(formatBuffer, Telephone_Data_80181A70);
            textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, valueText, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case TELEPHONE_PLAY_DATA_ESCAPES: {
            TextDrawReq captionRequest;
            s32         captionBaseY;

            TELEPHONE_DRAW_PLAY_DATA_CAPTION(Telephone_Data_80181A2C);
            textItoaUnsigned(formatBuffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesEscaped);
            textAppendString(formatBuffer, Telephone_Data_80181A70);
            textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, valueText, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case TELEPHONE_PLAY_DATA_WIN_PERCENT: {
            TextDrawReq captionRequest;
            s32         captionBaseY;
            s32         percentHundredths;
            s32         digitCount;
            s32         shiftBytes;
            s32         shiftIndex;
            u8*         digitEnd;

            TELEPHONE_DRAW_PLAY_DATA_CAPTION(Telephone_Data_80181A34);
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon == 0) {
                percentHundredths = 0;
            } else {
                percentHundredths = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon * TELEPHONE_PERCENT_WHOLE) / (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesEscaped);
            }
            if (percentHundredths < TELEPHONE_PERCENT_UNITS_PER_PERCENT) {
                textItoaPadded(formatBuffer, percentHundredths, TELEPHONE_PERCENT_FRACTIONAL_DIGITS + 1);
            } else {
                textItoaUnsigned(formatBuffer, percentHundredths);
            }
            shiftBytes = TELEPHONE_PERCENT_FRACTIONAL_DIGITS;
            digitEnd   = formatBuffer;
            digitCount = 0;
            TELEPHONE_SHIFT_DECIMAL_DIGITS(digitEnd, digitCount, shiftBytes, shiftIndex);
            textAppendString(formatBuffer, Telephone_Data_80181A78);
            textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, valueText, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case TELEPHONE_PLAY_DATA_EXTERMINATION_PERCENT: {
            TextDrawReq captionRequest;
            s32         captionBaseY;
            s32         percentHundredths;
            s32         killCount;
            s32         enemyTotal;
            s32         digitCount;
            s32         shiftBytes;
            s32         shiftIndex;
            u8*         digitEnd;

            killCount = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon;
            TELEPHONE_DRAW_PLAY_DATA_CAPTION(Telephone_Data_80181A40);
            enemyTotal = TELEPHONE_EXTERMINATION_TOTAL;
            killCount  = killCount + (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B) + gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_A));
            if (killCount == 0) {
                percentHundredths = 0;
            } else {
                percentHundredths = (killCount * TELEPHONE_PERCENT_WHOLE) / enemyTotal;
            }
            if (percentHundredths < TELEPHONE_PERCENT_UNITS_PER_PERCENT) {
                textItoaPadded(formatBuffer, percentHundredths, TELEPHONE_PERCENT_FRACTIONAL_DIGITS + 1);
            } else {
                textItoaUnsigned(formatBuffer, percentHundredths);
            }
            shiftBytes = TELEPHONE_PERCENT_FRACTIONAL_DIGITS;
            digitEnd   = formatBuffer;
            digitCount = 0;
            TELEPHONE_SHIFT_DECIMAL_DIGITS(digitEnd, digitCount, shiftBytes, shiftIndex);
            textAppendString(formatBuffer, Telephone_Data_80181A78);
            textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, valueText, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            uiDrawHorizontalSeparator(&(object)->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, list->rowTextY.signedValue + 3);
            list->rowTextY.signedValue = list->rowTextY.unsignedValue + 5;
            break;
        }
        case TELEPHONE_PLAY_DATA_CLEARS: {
            TextDrawReq captionRequest;
            s32         captionBaseY;

            TELEPHONE_DRAW_PLAY_DATA_CAPTION(Telephone_Data_80181A58);
            textItoaUnsigned(formatBuffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount);
            textAppendString(formatBuffer, Telephone_Data_80181A70);
            textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, valueText, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case TELEPHONE_PLAY_DATA_MAX_EXP: {
            TextDrawReq captionRequest;
            s32         captionBaseY;

            TELEPHONE_DRAW_PLAY_DATA_CAPTION(Telephone_Data_80181A60);
            textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, textItoaUnsigned(formatBuffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.maxExp),
                           list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case TELEPHONE_PLAY_DATA_MAX_BP: {
            TextDrawReq captionRequest;
            s32         captionBaseY;

            TELEPHONE_DRAW_PLAY_DATA_CAPTION(Telephone_Data_80181A68);
            textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, textItoaUnsigned(formatBuffer, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.maxBp),
                           list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
    }
#undef TELEPHONE_DRAW_PLAY_DATA_CAPTION
}

static const char Telephone_Data_8017D610[] = "Play Data";

static const u8 Telephone_Data_8017D61C[] = "100.0%";

/// Formats a sub-100-percent usage share in hundredths of a percent.
///
/// Borrows a writable buffer of at least seven bytes and a share in 0..9999.
/// Writes two fractional digits (0 becomes `0.00%`, 9999 becomes `99.99%`).
/// The text is NUL-terminated; no pointer is retained.
static inline void _telephoneFormatUsageShare(u8* formatBuffer, s32 usageShare)
{
    s32 wholePercentThreshold;
    s32 powerIndex;
    s32 shiftBytes;
    s32 digitCount;
    u8* digitEnd;

    wholePercentThreshold = 1;
    for (powerIndex = TELEPHONE_PERCENT_FRACTIONAL_DIGITS; powerIndex > 0; powerIndex--) {
        wholePercentThreshold *= 10;
    }
    if (usageShare < wholePercentThreshold) {
        textItoaPadded(formatBuffer, usageShare, TELEPHONE_PERCENT_FRACTIONAL_DIGITS + 1);
    } else {
        textItoaUnsigned(formatBuffer, usageShare);
    }
    shiftBytes = TELEPHONE_PERCENT_FRACTIONAL_DIGITS;
    digitEnd   = formatBuffer;
    digitCount = 0;
    TELEPHONE_SHIFT_DECIMAL_DIGITS(digitEnd, digitCount, shiftBytes, digitCount);
    textAppendString(formatBuffer, Telephone_Data_80181A78);
}

/// Queues the usage gauge's opaque red horizontal gradient.
///
/// Borrows a panel and pixel offsets/width; widths below two reserve nothing.
/// Requires room for one POLY_G4 and a live OT. Retains low 16 coordinate bits,
/// including an unsigned read of the left edge; the caller draws the bevel.
/// Starts one pixel inside each origin offset; the fill is nine pixels tall,
/// from RGB (176,0,1) at the left to (0,0,1) at the right, at panel OT depth + 1.
static inline void _telephoneDrawUsageGaugeFill(const UiPanel* panel, s32 gaugeX, s32 gaugeY, s32 gaugeWidth)
{
    enum { TELEPHONE_USAGE_GAUGE_PACKET_CODE = 0x38 };
    POLY_G4* gaugePrimitive;
    s32      edgeX;
    s32      edgeY;

    if (gaugeWidth >= 2) {
        gaugePrimitive                              = gGpuPrimCursor;
        edgeX                                       = panel->contentOriginX.unsignedValue + gaugeX + 1;
        gaugePrimitive->x2                          = edgeX;
        gaugePrimitive->x0                          = edgeX;
        edgeY                                       = panel->contentOriginY.unsignedValue;
        gGpuPrimCursor                              = gaugePrimitive + 1;
        edgeY                                       = edgeY + gaugeY;
        edgeY                                      += 1;
        GPU_PRIMITIVE_COLOR_WORD(gaugePrimitive, 3) = GPU_PACK_COLOR_WORD(0, 0, 0x01, 0);
        GPU_PRIMITIVE_COLOR_WORD(gaugePrimitive, 1) = GPU_PACK_COLOR_WORD(0, 0, 0x01, 0);
        setlen(gaugePrimitive, sizeof(*gaugePrimitive) / sizeof(u32) - 1);
        GPU_PRIMITIVE_COLOR_WORD(gaugePrimitive, 0) = GPU_PACK_COLOR_WORD(0xb0, 0, 0x01, 0);
        setcode(gaugePrimitive, TELEPHONE_USAGE_GAUGE_PACKET_CODE);
        GPU_PRIMITIVE_COLOR_WORD(gaugePrimitive, 2) = GPU_PACK_COLOR_WORD(0xb0, 0, 0x01, 0);
        edgeX                                       = (u16)gaugePrimitive->x0 + gaugeWidth - 1;
        gaugePrimitive->y1                          = edgeY;
        gaugePrimitive->y0                          = edgeY;
        edgeY                                      += TELEPHONE_USAGE_GAUGE_HEIGHT - 1;
        gaugePrimitive->y3                          = edgeY;
        gaugePrimitive->y2                          = edgeY;
        gaugePrimitive->x3                          = edgeX;
        gaugePrimitive->x1                          = edgeX;
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, gaugePrimitive);
    }
}

/// Draws a weapon/P.E. usage row and handles its preview/detail input.
///
/// Requires a live panel owner with populated usage work and current row in
/// 0..itemCount-1, at most TELEPHONE_USAGE_ROW_CAPACITY. Shares are hundredths
/// of a percent; shares at or above 100% use the fixed `100.0%` label. Gauges
/// are signed Q12 ratios to the most-used row. Draws the
/// percentage and gauge even while the caption/icon panel is hidden. An active
/// selected row updates item preview/help; active triangle input opens a detail
/// child and deactivates the parent. Requires loaded item text/icons, writable
/// frame GPU storage and a live panel ordering table; no storage is retained.
static void _telephoneDrawUsageRow(UiList* list, UiObject* object)
{
    enum {
        TELEPHONE_USAGE_ICON_TEXT_OFFSET      = 17,
        TELEPHONE_USAGE_GAUGE_LEFT_INSET      = 128,
        TELEPHONE_USAGE_GAUGE_RIGHT_INSET     = 74,
        TELEPHONE_USAGE_GAUGE_BASELINE_OFFSET = 12,
        TELEPHONE_USAGE_DETAIL_TEMPLATE       = 45
    };
    u8                         percentageText[0x20];
    TextDrawReq                captionRequest;
    u8*                        formatBuffer;
    TextDrawReq*               request;
    const _TelephoneUsageWork* work;
    s32                        itemId;
    s32                        usageShare;
    s32                        rowX;
    s32                        rowY;
    s32                        textColor;
    s32                        captionBaseY;
    s32                        gaugeRight;
    s32                        gaugeLeft;
    s32                        gaugeWidth;
    s32                        gaugeX;
    s32                        gaugeY;
    s32                        panelActive;

    formatBuffer = percentageText;
    request      = &captionRequest;
    rowX         = list->rowTextX.signedValue;
    rowY         = list->rowTextY.signedValue;
    work         = object->owner->work;
    itemId       = work->itemIds[list->currentItemIndex];
    usageShare   = work->usageShares[list->currentItemIndex];
    textColor    = list->colorRgb;
    if (object->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        captionRequest.x          = object->panel.contentOriginX.unsignedValue + TELEPHONE_USAGE_ICON_TEXT_OFFSET + rowX;
        captionBaseY              = object->panel.contentOriginY.unsignedValue - 6;
        captionRequest.y          = captionBaseY + rowY;
        captionRequest.otIndex    = object->panel.otIndex.signedValue + 1;
        captionRequest.colorRgb   = textColor;
        captionRequest.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        captionRequest.alignment  = TEXT_ALIGNMENT_LEFT;
        request->drawMode         = TEXT_DRAW_OUTLINED;
        textDrawString(request, itemGetText(itemId, ITEM_TEXT_NAME, 0));
        itemMenuDrawDefaultItemIcon(object, rowX, rowY, itemId);
    }
    if (usageShare >= TELEPHONE_PERCENT_WHOLE) {
        textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, Telephone_Data_8017D61C, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    } else {
        _telephoneFormatUsageShare(formatBuffer, usageShare);
        textDrawUiLine(object, -list->rowTextX.signedValue, list->rowTextY.signedValue, percentageText, list->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    }

    // Right-anchor the Q12 usage gauge, preserving unsigned packet coordinates.
    gaugeLeft  = object->panel.contentLeft.signedValue + TELEPHONE_USAGE_GAUGE_LEFT_INSET;
    gaugeRight = object->panel.contentRight.signedValue - TELEPHONE_USAGE_GAUGE_RIGHT_INSET;
    gaugeWidth = gaugeRight - gaugeLeft;
    gaugeWidth = (gaugeWidth * work->gaugeFractions[list->currentItemIndex]) >> TELEPHONE_GAUGE_FRACTION_BITS;
    gaugeY     = list->rowTextY.signedValue - TELEPHONE_USAGE_GAUGE_BASELINE_OFFSET;
    gaugeWidth = gaugeWidth + 2;
    gaugeX     = gaugeRight - gaugeWidth;
    _telephoneDrawUsageGaugeFill(&object->panel, gaugeX, gaugeY, gaugeWidth);
    panelActive = USER_INTERFACE_PANEL_ACTIVE;
    uiDrawBeveledRect(&(object)->panel, gaugeX, list->rowTextY.signedValue - TELEPHONE_USAGE_GAUGE_BASELINE_OFFSET, gaugeWidth, TELEPHONE_USAGE_GAUGE_HEIGHT, 0, panelActive);
    if (((object->panel.control.word >> TELEPHONE_SUSPENDED_CONTROL_SHIFT) == panelActive) || (object->panel.control.word == panelActive)) {
        if (list->selectedItemIndex == list->currentItemIndex) {
            itemMenuSetPreviewItem(itemId, CD_COMMAND_DISPLAY_LOAD_MENU);
            itemMenuSetItemDescriptionPrompt(itemId);
        }
    }
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EAB4[TELEPHONE_USAGE_DETAIL_TEMPLATE], itemId, USER_INTERFACE_PANEL_ACTIVE, 1, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

/// Moves one usage id to an earlier row, shifting the intervening ids.
///
/// Borrows writable item ids; 0 <= destinationIndex < sourceIndex < itemCount
/// <= TELEPHONE_USAGE_ROW_CAPACITY. Preserves the intervening rows' order.
/// Builders call before computing shares and gauges, so those arrays stay
/// untouched and the list count does not change.
static inline void _telephoneInsertUsageRow(_TelephoneUsageWork* work, s32 sourceIndex, s32 destinationIndex)
{
    s32 insertedItemId;
    s32 moveIndex;

    insertedItemId = work->itemIds[sourceIndex];
    for (moveIndex = sourceIndex - 1; moveIndex >= destinationIndex; moveIndex--) {
        work->itemIds[moveIndex + 1] = work->itemIds[moveIndex];
    }
    work->itemIds[destinationIndex] = insertedItemId;
}

/// Builds the weapon usage rows in descending recorded-use order.
///
/// Requires the owning task's zeroed `_TelephoneUsageWork`; fills at most 32 rows
/// and resets the list viewport and selection. Only positive counters with a
/// nonempty weapon name participate, and those weapons become identified.
/// Shares are rounded to hundredths of a percent; gauges use 12 fractional bits
/// relative to the first row. Counters and their sum must fit positive s32
/// arithmetic; normal recorded weapon counts are capped at 99999.
static void _telephoneBuildWeaponUsageList(UiList* list, const UiObject* object)
{
    _TelephoneUsageWork* work;
    s32                  rowCount;
    s32                  totalUses;
    s32                  index;
    s32                  insertIndex;
    s32                  itemId;
    s32                  insertionUses;
    s32                  shareScale;
    s32                  topUses;
    s32                  gaugeShift;
    s16*                 itemWrite;
    u8                   nameFirstByte;

    rowCount  = 0;
    totalUses = 0;
    work      = object->owner->work;
    itemWrite = work->itemIds;

    for (index = 0; index < ARRAY_SIZE(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts); index++) {
        itemId        = index + EQUIPMENT_WEAPON_ITEM_FIRST;
        nameFirstByte = *itemGetText(itemId, ITEM_TEXT_NAME, 1);
        if ((nameFirstByte != 0) && (nameFirstByte != '\n') && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[index] > 0)) {
            itemSetIdentified(itemId, 1);
            *itemWrite++ = itemId;
            rowCount++;
            totalUses += gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[index];
        }
    }

    // Stable descending insertion sort keeps catalogue order for equal counts.
    if (rowCount >= 2) {
        for (index = 1; index < rowCount; index++) {
            insertionUses = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[index] - EQUIPMENT_WEAPON_ITEM_FIRST];
            for (insertIndex = 0; insertIndex < index; insertIndex++) {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[insertIndex] - EQUIPMENT_WEAPON_ITEM_FIRST] < insertionUses) {
                    _telephoneInsertUsageRow(work, index, insertIndex);
                    break;
                }
            }
        }
    }

    // Reduce both denominators with their scales before fixed-point conversion.
    if (rowCount > 0) {
        shareScale = 2 * TELEPHONE_PERCENT_WHOLE;
        topUses    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[0] - EQUIPMENT_WEAPON_ITEM_FIRST];
        gaugeShift = TELEPHONE_GAUGE_FRACTION_BITS;
        while (topUses > TELEPHONE_MAX_UNSCALED_USE_COUNT) {
            topUses    >>= 1;
            shareScale >>= 1;
            totalUses  >>= 1;
            gaugeShift--;
        }
        for (index = 0; index < rowCount; index++) {
            work->usageShares[index] =
                (u32)((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[index] - EQUIPMENT_WEAPON_ITEM_FIRST] * shareScale) / totalUses + 1) >> 1;
            work->gaugeFractions[index] =
                (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[index] - EQUIPMENT_WEAPON_ITEM_FIRST] << gaugeShift) / topUses;
        }
    }

    list->itemCount                           = rowCount;
    list->firstVisibleItemIndex.unsignedValue = 0;
    list->selectedItemIndex                   = 0;
}

/// Builds the twelve spell-usage rows in descending recorded-use order.
///
/// Requires the owning task's zeroed `_TelephoneUsageWork`. Positive counters in
/// the first twelve ability slots participate; extra save slots are excluded.
/// Each row names the spell's acquired level (1..3), or level one when unlearned.
/// Equal counts retain ability order. Shares are rounded to hundredths of a
/// percent; gauges use 12 fractional bits relative to the first row. The list
/// viewport and selection reset to zero. Counters and their sum must fit positive
/// s32 arithmetic; no work storage is allocated or released here.
static void _telephoneBuildPeUsageList(UiList* list, const UiObject* object)
{
    _TelephoneUsageWork* work;
    s16*                 itemWrite;
    s32                  rowCount;
    s32                  totalUses;
    s32                  index;
    s32                  insertIndex;
    s32                  itemId;
    s32                  abilityIndex;
    s32                  insertionUses;
    s32                  shareScale;
    s32                  gaugeShift;
    s32                  topUses;

    rowCount  = 0;
    totalUses = 0;
    index     = 0;
    work      = object->owner->work;
    itemWrite = work->itemIds;

    for (; index < ATTACHMENT_SPELL_COUNT; index++) {
        s32 useCount;

        useCount = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[index];
        itemId   = index * ATTACHMENT_AREA_LEVEL_COUNT + TELEPHONE_PE_ITEM_FIRST;
        if (useCount > 0) {
            s32 abilityPage;
            s32 pageSlot;

            abilityPage = index / TELEPHONE_PE_SLOTS_PER_PAGE;
            pageSlot    = index % TELEPHONE_PE_SLOTS_PER_PAGE;
            *itemWrite  = itemId;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[pageSlot + abilityPage * TELEPHONE_PE_SLOTS_PER_PAGE] != 0) {
                *itemWrite = itemId + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[pageSlot + abilityPage * TELEPHONE_PE_SLOTS_PER_PAGE] - 1u);
            }
            itemWrite++;
            rowCount++;
            totalUses += gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[index];
        }
    }

    // Stable descending insertion sort keeps catalogue order for equal counts.
    if (rowCount >= 2) {
        for (index = 1; index < rowCount; index++) {
            abilityIndex  = (work->itemIds[index] - TELEPHONE_PE_ITEM_FIRST) / ATTACHMENT_AREA_LEVEL_COUNT;
            insertionUses = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[abilityIndex];
            for (insertIndex = 0; insertIndex < index; insertIndex++) {
                abilityIndex = (work->itemIds[insertIndex] - TELEPHONE_PE_ITEM_FIRST) / ATTACHMENT_AREA_LEVEL_COUNT;
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[abilityIndex] < insertionUses) {
                    _telephoneInsertUsageRow(work, index, insertIndex);
                    break;
                }
            }
        }
    }

    // Reduce both denominators with their scales before fixed-point conversion.
    if (rowCount > 0) {
        shareScale   = 2 * TELEPHONE_PERCENT_WHOLE;
        abilityIndex = (work->itemIds[0] - TELEPHONE_PE_ITEM_FIRST) / ATTACHMENT_AREA_LEVEL_COUNT;
        topUses      = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[abilityIndex];
        gaugeShift   = TELEPHONE_GAUGE_FRACTION_BITS;
        while (topUses > TELEPHONE_MAX_UNSCALED_USE_COUNT) {
            topUses    >>= 1;
            shareScale >>= 1;
            totalUses  >>= 1;
            gaugeShift--;
        }
        for (index = 0; index < rowCount; index++) {
            abilityIndex                = (work->itemIds[index] - TELEPHONE_PE_ITEM_FIRST) / ATTACHMENT_AREA_LEVEL_COUNT;
            work->usageShares[index]    = (u32)((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[abilityIndex] * shareScale) / totalUses + 1) >> 1;
            abilityIndex                = (work->itemIds[index] - TELEPHONE_PE_ITEM_FIRST) / ATTACHMENT_AREA_LEVEL_COUNT;
            work->gaugeFractions[index] = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[abilityIndex] << gaugeShift) / topUses;
        }
    }

    list->itemCount                           = rowCount;
    list->firstVisibleItemIndex.unsignedValue = 0;
    list->selectedItemIndex                   = 0;
}

static const char Telephone_Data_8017D624[] = "Weapon Data";

static const char Telephone_Data_8017D630[] = "PE Data";

/// Updates a weapon or Parasite Energy usage panel and its child windows.
///
/// The task owns the UI object in `spawnArg2.pointer`; `spawnArg1.value` selects
/// weapons (0) or spells (nonzero). First-frame allocation creates the usage work
/// freed by normal task teardown; allocation failure retries on a later frame.
/// Initialization opens a help panel and builds the rows once. Active cancel
/// publishes Confirm so the parent reopens its menu. Finished detail/help children
/// close as trees and restore this panel's input control.
static void _telephoneUsageTask(Task* task)
{
    UiObject*            object;
    UiList*              list;
    Task*                childTask;
    Task*                nextSibling;
    UiObject*            childObject;
    _TelephoneUsageWork* work;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    list           = &Telephone_Data_80181C6C;
    if (task->spawnArg1.value == TELEPHONE_USAGE_WEAPONS) {
        uiDrawPanelLabel(&(object)->panel, Telephone_Data_8017D624);
    } else {
        uiDrawPanelLabel(&(object)->panel, Telephone_Data_8017D630);
    }
    // Keep initialization pending when work allocation fails.
    if (task->state == TELEPHONE_PANEL_INITIALIZE) {
        work = memCalloc(sizeof(_TelephoneUsageWork), 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        uiSpawnObject(&Telephone_Data_80181C90, 0, USER_INTERFACE_PANEL_INACTIVE, 1, object);
        if (task->spawnArg1.value == TELEPHONE_USAGE_WEAPONS) {
            _telephoneBuildWeaponUsageList(list, object);
        } else {
            _telephoneBuildPeUsageList(list, object);
        }
        uiInitList(list, &(object)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
        object->result = USER_INTERFACE_RESULT_CONFIRM;
    }
    // Save the next sibling before requesting child-tree closure.
    if (task->firstChild != NULL) {
        childTask = task->firstChild;
        do {
            childObject = childTask->spawnArg2.pointer;
            nextSibling = childTask->nextSibling;
            if (childObject->result == USER_INTERFACE_RESULT_CANCEL || childObject->result == USER_INTERFACE_RESULT_CONFIRM) {
                uiStartTreeClosing(childObject, childObject->owner);
                object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            }
            childTask = nextSibling;
        } while (childTask != task->firstChild);
    }
}

static const char Telephone_Data_8017D638[12] = TELEPHONE_TITLE_BYTES;

/// Updates the telephone menu and routes save and statistics dismissal.
///
/// The task owns the UI object in `spawnArg2.pointer`. A clear or attract-demo
/// scene 1 enables the four-row menu; otherwise initialization opens the save
/// dialog directly. Save completion selects a saved/cancelled notice from the
/// child answer, then returns to the menu or closes the save-only telephone.
/// Cancel publishes the No command and clears the session's UI-open flag.
/// Only the first child supplies this controller's result.
static inline void _telephoneMenuTask(Task* task)
{
    enum { TELEPHONE_STATISTICS_DEMO_SCENE = 1 };
    UiObject* object;
    UiList*   list;
    Task*     childTask;
    UiObject* childObject;
    s32       showStatistics;
    s32       childResult;
    s32       saveAnswer;
    s32       noticeId;
    s32       activeControl;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    showStatistics = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == TELEPHONE_STATISTICS_DEMO_SCENE;
    list           = &Telephone_Data_80181CF4;
    activeControl  = USER_INTERFACE_PANEL_ACTIVE;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
        showStatistics = activeControl;
    }
    // Before a clear, normal play enters the save dialog directly.
    if (showStatistics == 0) {
        if (task->state == TELEPHONE_MENU_INITIALIZE) {
            gGameSession->uiOpen = activeControl;
            uiSpawnObject(&D_800611E4, 0, USER_INTERFACE_PANEL_INACTIVE, 0, object);
            object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            object->panel.style       |= USER_INTERFACE_PANEL_NO_FRAME;
            task->state                = task->state + 1;
        }
    } else if (task->state == TELEPHONE_MENU_INITIALIZE) {
        uiFitPanelToList(list, &(object)->panel);
        object->panel.control.word = activeControl;
        gGameSession->uiOpen       = activeControl;
        uiSetListSystemCursorSound(list, 1);
        itemMenuClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        uiDrawPanelLabel(&(object)->panel, Telephone_Data_8017D638);
        uiUpdateList(list, &object->panel);
    }
    if (object->result == USER_INTERFACE_RESULT_CONFIRM) {
        object->result = USER_INTERFACE_RESULT_NONE;
        uiStartPanelHiding(object, task);
        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
        if (task->state != TELEPHONE_MENU_INITIALIZE) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
        }
        gGameSession->uiOpen = 0;
        object->result       = USER_INTERFACE_RESULT_CANCEL;
        object->resultValue  = USER_INTERFACE_LIST_COMMAND_NO;
    }
    // A save answer gets a notice; statistics dismissal reopens the list.
    childTask = task->firstChild;
    if (childTask != NULL) {
        childObject = childTask->spawnArg2.pointer;
        childResult = childObject->result;
        switch (childResult) {
            case USER_INTERFACE_RESULT_CONFIRM:
                if (task->state == TELEPHONE_MENU_WAIT_SAVE) {
                    saveAnswer = childObject->resultValue;
                    uiStartTreeClosing(childObject, childObject->owner);
                    noticeId = ITEM_MENU_NOTICE_SAVE_CANCELLED;
                    if (saveAnswer == USER_INTERFACE_LIST_COMMAND_YES) {
                        noticeId = ITEM_MENU_NOTICE_SAVE_COMPLETE;
                    }
                    itemMenuSpawnNotice(object, noticeId, 0, ITEM_MENU_NOTICE_RESULT_CONFIRM);
                    if (showStatistics == 0) {
                        task->state = TELEPHONE_MENU_WAIT_NOTICE_AND_CLOSE;
                    } else {
                        task->state = TELEPHONE_MENU_WAIT_STATISTICS;
                    }
                } else if (task->state == TELEPHONE_MENU_WAIT_NOTICE_AND_CLOSE) {
                    object->result      = USER_INTERFACE_RESULT_CANCEL;
                    object->resultValue = USER_INTERFACE_LIST_COMMAND_NO;
                } else {
                    uiStartTreeClosing(childObject, childObject->owner);
                    sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
                    uiStartPanelOpening(&(object)->panel, task);
                    object->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                }
                break;
            case USER_INTERFACE_RESULT_CANCEL:
                if (task->state == TELEPHONE_MENU_WAIT_SAVE) {
                    saveAnswer = childObject->resultValue;
                    uiStartTreeClosing(childObject, childObject->owner);
                    noticeId = ITEM_MENU_NOTICE_SAVE_CANCELLED;
                    if (saveAnswer == USER_INTERFACE_LIST_COMMAND_YES) {
                        noticeId = ITEM_MENU_NOTICE_SAVE_COMPLETE;
                    }
                    itemMenuSpawnNotice(object, noticeId, 0, ITEM_MENU_NOTICE_RESULT_CONFIRM);
                    if (showStatistics == 0) {
                        task->state = TELEPHONE_MENU_WAIT_NOTICE_AND_CLOSE;
                    } else {
                        task->state = TELEPHONE_MENU_WAIT_STATISTICS;
                    }
                } else {
                    object->result      = USER_INTERFACE_RESULT_CANCEL;
                    object->resultValue = USER_INTERFACE_LIST_COMMAND_NO;
                }
                break;
        }
    }
}
