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

static void Telephone_DrawUsageRow(UiList* arg0, UiObject* arg1);

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

/// Draws one row of a usage panel from the `_TelephoneUsageWork` block in the
/// owning task's work area: the item's name, its share of all recorded uses as
/// a percentage with two decimals, and a gauge scaled by the row's
/// `gaugeFractions` entry. Highlighting the row previews the item; pressing the
/// detail button on the selected row opens the item's detail window.
static void Telephone_DrawUsageRow(UiList* arg0, UiObject* arg1)
{
    u8                   buf[0x20];
    TextDrawReq          req;
    TextDrawReq*         request;
    _TelephoneUsageWork* work;
    POLY_G4*             prim;
    u8*                  p;
    u8*                  q;
    s32                  item;
    s32                  value;
    s32                  x;
    s32                  y;
    s32                  color;
    s32                  textY;
    s32                  limit;
    s32                  n;
    s32                  len;
    s32                  i;
    s32                  avail;
    s32                  base;
    s32                  barW;
    s32                  barX;
    s32                  rowY;
    s32                  one;
    s32                  tx;
    s32                  ty;

    p       = buf;
    request = &req;
    x       = arg0->rowTextX.signedValue;
    y       = arg0->rowTextY.signedValue;
    work    = arg1->owner->work;
    item    = work->itemIds[arg0->currentItemIndex];
    value   = work->usageShares[arg0->currentItemIndex];
    color   = arg0->colorRgb;
    if (arg1->panel.state != USER_INTERFACE_PANEL_HIDDEN) {
        req.x             = arg1->panel.contentOriginX.unsignedValue + 0x11 + x;
        textY             = arg1->panel.contentOriginY.unsignedValue - 6;
        req.y             = textY + y;
        req.otIndex       = arg1->panel.otIndex.signedValue + 1;
        req.colorRgb      = color;
        req.glyphTable    = TEXT_GLYPH_TABLE_MEDIUM;
        req.alignment     = TEXT_ALIGNMENT_LEFT;
        request->drawMode = TEXT_DRAW_OUTLINED;
        textDrawString(request, itemGetText(item, ITEM_TEXT_NAME, 0));
        itemMenuDrawDefaultItemIcon(arg1, x, y, item);
    }
    limit = 1;
    if (value >= 10000) {
        textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, Telephone_Data_8017D61C, arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    } else {
        for (i = 2; i > 0; i--) {
            limit *= 10;
        }
        if (value < limit) {
            textItoaPadded(p, value, 3);
        } else {
            textItoaUnsigned(p, value);
        }
        n   = 2;
        q   = p;
        len = 0;
        while (*q != 0) {
            q++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            q[1] = q[0];
            q--;
        }
        q[1] = '.';
        textAppendString(p, Telephone_Data_80181A78);
        textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, buf, arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
    }

    base  = arg1->panel.contentLeft.signedValue + 0x80;
    avail = arg1->panel.contentRight.signedValue - 0x4A;
    barW  = avail - base;
    barW  = (barW * work->gaugeFractions[arg0->currentItemIndex]) >> 12;
    rowY  = arg0->rowTextY.signedValue - 0xC;
    barW  = barW + 2;
    barX  = avail - barW;
    if (barW >= 2) {
        prim                              = gGpuPrimCursor;
        tx                                = arg1->panel.contentOriginX.unsignedValue + barX + 1;
        prim->x2                          = tx;
        prim->x0                          = tx;
        ty                                = arg1->panel.contentOriginY.unsignedValue;
        gGpuPrimCursor                    = prim + 1;
        ty                                = ty + rowY;
        ty                               += 1;
        GPU_PRIMITIVE_COLOR_WORD(prim, 3) = GPU_PACK_COLOR_WORD(0, 0, 0x01, 0);
        GPU_PRIMITIVE_COLOR_WORD(prim, 1) = GPU_PACK_COLOR_WORD(0, 0, 0x01, 0);
        setlen(prim, 8);
        GPU_PRIMITIVE_COLOR_WORD(prim, 0) = GPU_PACK_COLOR_WORD(0xb0, 0, 0x01, 0);
        setcode(prim, 0x38);
        GPU_PRIMITIVE_COLOR_WORD(prim, 2) = GPU_PACK_COLOR_WORD(0xb0, 0, 0x01, 0);
        tx                                = (u16)prim->x0 + barW - 1;
        prim->y1                          = ty;
        prim->y0                          = ty;
        ty                               += 8;
        prim->y3                          = ty;
        prim->y2                          = ty;
        prim->x3                          = tx;
        prim->x1                          = tx;
        addPrim(gGpuCurrentOt + arg1->panel.otIndex.signedValue + 1, prim);
    }
    one = 1;
    uiDrawBeveledRect(&(arg1)->panel, barX, arg0->rowTextY.signedValue - 0xC, barW, 9, 0, one);
    if (((arg1->panel.control.word >> 16) == one) || (arg1->panel.control.word == one)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            itemMenuSetPreviewItem(item, CD_COMMAND_DISPLAY_LOAD_MENU);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EAB4[45], item, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

/// Moves one usage id to an earlier row, shifting the intervening ids.
///
/// Both indices are live rows; 0 <= destinationIndex < sourceIndex < itemCount.
static inline void _telephoneInsertUsageRow(_TelephoneUsageWork* work, s32 sourceIndex, s32 destinationIndex)
{
    s32 sortedItemId;
    s32 moveIndex;

    sortedItemId = work->itemIds[sourceIndex];
    for (moveIndex = sourceIndex - 1; moveIndex >= destinationIndex; moveIndex--) {
        work->itemIds[moveIndex + 1] = work->itemIds[moveIndex];
    }
    work->itemIds[destinationIndex] = sortedItemId;
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
