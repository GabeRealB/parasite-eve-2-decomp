#include "telephone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"
#include "types.h"

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

static void Telephone_DrawPlayDataRow(UiList* arg0, UiObject* arg1);
static void Telephone_DrawUsageRow(UiList* arg0, UiObject* arg1);
static void Telephone_UsageTask(Task* task);
static void Telephone_PromptTask(Task* task);
static void Telephone_PlayDataTask(Task* task);
static void Telephone_SaveRow(UiList* prompt, UiObject* obj);
static void Telephone_PlayDataRow(UiList* prompt, UiObject* obj);
static void Telephone_WeaponDataRow(UiList* prompt, UiObject* obj);
static void Telephone_PeDataRow(UiList* prompt, UiObject* obj);

static void Telephone_BuildWeaponUsage(UiList* list, UiObject* obj);
static void Telephone_BuildPeUsage(UiList* list, UiObject* obj);
static void Telephone_InsertDecimalPoint(u8* str, s32 decimals);
static u8*  Telephone_FormatPercentage(u8* buf, s32 value, s32 decimals);
static void Telephone_DrawGauge(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6);
static void Telephone_ClosePrompt(Task* task);

/// Draws one row of the play-data panel, the row picked by
/// `UiList::currentItemIndex`: a caption followed by a value - play time, one of
/// several counters with a unit suffix, or a percentage kept in hundredths
/// whose decimal point is inserted by hand (row 5 also draws a gauge and takes
/// an extra line). While the cursor is on the row its help string is shown.
static void Telephone_DrawPlayDataRow(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.control.word >> 16) == USER_INTERFACE_PANEL_ACTIVE) || (arg1->panel.control.word == USER_INTERFACE_PANEL_ACTIVE)) {
        if (arg0->selectedItemIndex == arg0->currentItemIndex) {
            u8* tbl[9] = {
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

            uiSetPromptText(tbl[arg0->currentItemIndex], 0, 0);
        }
    }

    switch (arg0->currentItemIndex) {
        case 0: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
            y              = arg1->panel.contentOriginY.unsignedValue - 6;
            req.y          = arg0->rowTextY.unsignedValue + y;
            req.otIndex    = arg1->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg0->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Telephone_Data_80181A20);
            textFormatPlayTime(p, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime);
            textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, buf, arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case 1: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
            y              = arg1->panel.contentOriginY.unsignedValue - 6;
            req.y          = arg0->rowTextY.unsignedValue + y;
            req.otIndex    = arg1->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg0->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Telephone_Data_80181A50);
            textItoaUnsigned(p, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.saveCount);
            textAppendString(p, Telephone_Data_80181A70);
            textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, buf, arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case 2: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
            y              = arg1->panel.contentOriginY.unsignedValue - 6;
            req.y          = arg0->rowTextY.unsignedValue + y;
            req.otIndex    = arg1->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg0->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Telephone_Data_80181A28);
            textItoaUnsigned(p, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon);
            textAppendString(p, Telephone_Data_80181A70);
            textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, buf, arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case 3: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
            y              = arg1->panel.contentOriginY.unsignedValue - 6;
            req.y          = arg0->rowTextY.unsignedValue + y;
            req.otIndex    = arg1->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg0->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Telephone_Data_80181A2C);
            textItoaUnsigned(p, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesEscaped);
            textAppendString(p, Telephone_Data_80181A70);
            textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, buf, arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case 4: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
            y              = arg1->panel.contentOriginY.unsignedValue - 6;
            req.y          = arg0->rowTextY.unsignedValue + y;
            req.otIndex    = arg1->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg0->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Telephone_Data_80181A34);
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon == 0) {
                pct = 0;
            } else {
                pct = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon * 10000) / (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesEscaped);
            }
            if (pct < 100) {
                textItoaPadded(p, pct, 3);
            } else {
                textItoaUnsigned(p, pct);
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
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            textAppendString(p, Telephone_Data_80181A78);
            textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, buf, arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case 5: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         total;
            s32         cnt;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            total          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.battlesWon;
            req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
            y              = arg1->panel.contentOriginY.unsignedValue - 6;
            req.y          = arg0->rowTextY.unsignedValue + y;
            req.otIndex    = arg1->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg0->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Telephone_Data_80181A40);
            cnt   = 326;
            total = total + (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_B) + gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_A));
            if (total == 0) {
                pct = 0;
            } else {
                pct = (total * 10000) / cnt;
            }
            if (pct < 100) {
                textItoaPadded(p, pct, 3);
            } else {
                textItoaUnsigned(p, pct);
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
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            textAppendString(p, Telephone_Data_80181A78);
            textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, buf, arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            uiDrawHorizontalSeparator(&(arg1)->panel, arg1->panel.contentLeft.signedValue, arg1->panel.contentRight.signedValue, arg0->rowTextY.signedValue + 3);
            arg0->rowTextY.signedValue = arg0->rowTextY.unsignedValue + 5;
            break;
        }
        case 6: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
            y              = arg1->panel.contentOriginY.unsignedValue - 6;
            req.y          = arg0->rowTextY.unsignedValue + y;
            req.otIndex    = arg1->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg0->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Telephone_Data_80181A58);
            textItoaUnsigned(p, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount);
            textAppendString(p, Telephone_Data_80181A70);
            textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, buf, arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case 7: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
            y              = arg1->panel.contentOriginY.unsignedValue - 6;
            req.y          = arg0->rowTextY.unsignedValue + y;
            req.otIndex    = arg1->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg0->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Telephone_Data_80181A60);
            textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, textItoaUnsigned(p, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.maxExp),
                           arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
        case 8: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.contentOriginX.unsignedValue + arg0->rowTextX.unsignedValue;
            y              = arg1->panel.contentOriginY.unsignedValue - 6;
            req.y          = arg0->rowTextY.unsignedValue + y;
            req.otIndex    = arg1->panel.otIndex.signedValue + 1;
            req.colorRgb   = arg0->colorRgb;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_LEFT;
            req.drawMode   = TEXT_DRAW_OUTLINED;
            textDrawString(&req, Telephone_Data_80181A68);
            textDrawUiLine(arg1, -arg0->rowTextX.signedValue, arg0->rowTextY.signedValue, textItoaUnsigned(p, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.maxBp),
                           arg0->colorRgb, TEXT_DRAW_TRANSLUCENT_OUTLINED, TEXT_ALIGNMENT_RIGHT);
            break;
        }
    }
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
        func_800CE5D0(arg1, x, y, item);
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
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg0->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_TRIANGLE) != 0) {
            sndEvtRequestScriptStart(SOUND_MENU_CONFIRM, 0, 0);
            uiSpawnObject(&D_8010EFA0, item, 1, 1, arg1);
            arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        }
    }
}

/// Builds the "Play Data" item-usage panel's three parallel arrays from the
/// save's per-item use counters (`gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts`, ids 0x80-0x9F).
///
/// Every id whose name is non-empty (a leading 0 or 0xA marks an unused row)
/// and whose counter is non-zero is marked seen and appended to `itemIds`,
/// while the counters are summed. The ids are then insertion-sorted by use
/// count, most-used first. Finally each row gets `usageShares` - its share of
/// all recorded uses in hundredths of a percent, rounded - and
/// `gaugeFractions`, its counter as a 12-bit fraction of the top row's. Both
/// are scaled down by halving until the top counter fits in 17 bits, so the
/// multiply and the shift cannot overflow.
static void Telephone_BuildWeaponUsage(UiList* list, UiObject* obj)
{
    _TelephoneUsageWork* work;
    s32                  count;
    s32                  total;
    s32                  i;
    s32                  j;
    s32                  k;
    s32                  id;
    s32                  tmp;
    s32                  uses;
    s32                  scale;
    s32                  top;
    s32                  shift;
    s16*                 p;
    u8                   c;

    count = 0;
    total = 0;
    work  = obj->owner->work;
    p     = work->itemIds;

    for (i = 0; i < 0x20; i++) {
        id = i + 0x80;
        c  = *itemGetText(id, ITEM_TEXT_NAME, 1);
        if ((c != 0) && (c != 0xA) && (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[i] > 0)) {
            Gp_SetItemSeenBit(id, 1);
            *p++ = id;
            count++;
            total += gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            uses = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[i] - 0x80];
            for (j = 0; j < i; j++) {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[j] - 0x80] < uses) {
                    tmp = work->itemIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->itemIds[k + 1] = work->itemIds[k];
                    }
                    work->itemIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        top   = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[0] - 0x80];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            work->usageShares[i] =
                (u32)((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[i] - 0x80] * scale) / total + 1) >> 1;
            work->gaugeFractions[i] =
                (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[work->itemIds[i] - 0x80] << shift) / top;
        }
    }

    list->itemCount                           = count;
    list->firstVisibleItemIndex.unsignedValue = 0;
    list->selectedItemIndex                   = 0;
}

/// Parasite Energy counterpart of `Telephone_BuildWeaponUsage`: fills
/// the "Play Data" PE-usage panel's `_TelephoneUsageWork` block from the save's
/// per-slot use counters.
///
/// Each of the twelve Parasite Energy slots owns three consecutive ids starting
/// at 0xF, one per level, so slot `i` at level `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[i]`
/// prints as `i * 3 + 0xF + level - 1` (a slot the player has never levelled
/// keeps the base id). Every slot with a non-zero counter in
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts` is appended and its counter summed. Levels
/// are addressed by page and column, with three slots per page. The ids are
/// then insertion-sorted by use count, most-used first, and each row gets
/// `usageShares`, its share of all recorded uses in hundredths of a percent, and
/// `gaugeFractions`, its counter as a 12-bit fraction of the top row's. Both are
/// scaled down by halving until the top counter fits in 17 bits, so the
/// multiply and the shift cannot overflow.
static void Telephone_BuildPeUsage(UiList* list, UiObject* obj)
{
    _TelephoneUsageWork* work;
    s16*                 p;
    s32                  count;
    s32                  total;
    s32                  i;
    s32                  j;
    s32                  k;
    s32                  id;
    s32                  slot;
    s32                  uses;
    s32                  scale;
    s32                  shift;
    s32                  top;
    s32                  tmp;

    count = 0;
    total = 0;
    i     = 0;
    work  = obj->owner->work;
    p     = work->itemIds;

    for (; i < 12; i++) {
        s32 useCount;

        useCount = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[i];
        id       = i * 3 + 0xF;
        if (useCount > 0) {
            s32 page;
            s32 column;

            page   = i / 3;
            column = i % 3;
            *p     = id;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[column + page * 3] != 0) {
                *p = id + (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels[column + page * 3] - 1u);
            }
            p++;
            count++;
            total += gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            slot = (work->itemIds[i] - 0xF) / 3;
            uses = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[slot];
            for (j = 0; j < i; j++) {
                slot = (work->itemIds[j] - 0xF) / 3;
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[slot] < uses) {
                    tmp = work->itemIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->itemIds[k + 1] = work->itemIds[k];
                    }
                    work->itemIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        slot  = (work->itemIds[0] - 0xF) / 3;
        top   = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[slot];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            slot                    = (work->itemIds[i] - 0xF) / 3;
            work->usageShares[i]    = (u32)((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[slot] * scale) / total + 1) >> 1;
            slot                    = (work->itemIds[i] - 0xF) / 3;
            work->gaugeFractions[i] = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachUseCounts[slot] << shift) / top;
        }
    }

    list->itemCount                           = count;
    list->firstVisibleItemIndex.unsignedValue = 0;
    list->selectedItemIndex                   = 0;
}

static const char Telephone_Data_8017D624[] = "Weapon Data";

static const char Telephone_Data_8017D630[] = "PE Data";

/// Task body of the usage panel: `spawnArg1` 0 lists weapons, anything else
/// Parasite Energy. On its first frame it allocates the row block, spawns the
/// row descriptor and fills the list; every frame it updates the list, closes
/// on cancel, and tears down any child window that has finished.
static void Telephone_UsageTask(Task* task)
{
    UiObject*            obj;
    UiList*              list;
    Task*                child;
    Task*                next;
    UiObject*            childObj;
    _TelephoneUsageWork* work;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    list        = &Telephone_Data_80181C6C;
    if (task->spawnArg1.value == 0) {
        uiDrawPanelLabel(&(obj)->panel, Telephone_Data_8017D624);
    } else {
        uiDrawPanelLabel(&(obj)->panel, Telephone_Data_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(sizeof(_TelephoneUsageWork), 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        uiSpawnObject(&Telephone_Data_80181C90, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            Telephone_BuildWeaponUsage(list, obj);
        } else {
            Telephone_BuildPeUsage(list, obj);
        }
        uiInitList(list, &(obj)->panel);
        list->flags = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        task->state += 1;
    }
    uiUpdateList(list, &obj->panel);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2.pointer;
            next     = child->nextSibling;
            if (childObj->result == USER_INTERFACE_RESULT_CANCEL || childObj->result == USER_INTERFACE_RESULT_CONFIRM) {
                uiStartTreeClosing(childObj, childObj->owner);
                obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

static const char Telephone_Data_8017D638[12] = TELEPHONE_TITLE_BYTES;

/// Task body of the telephone menu. Until the save has a clear or has reached
/// demo scene 1 it spawns `D_800611E4` in place of the list; otherwise it lays
/// out and updates the list. When the first child window finishes, the menu
/// opens the item prompt its selection picks, or closes.
static inline void Telephone_MenuTask(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s32       ready;
    s32       sel;
    s32       kind;
    s32       mode;
    s32       one;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    ready       = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene == 1;
    list        = &Telephone_Data_80181CF4;
    one         = 1;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            gGameSession->uiOpen = one;
            uiSpawnObject(&D_800611E4, 0, 0, 0, obj);
            obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
            obj->panel.style       |= USER_INTERFACE_PANEL_NO_FRAME;
            task->state             = task->state + 1;
        }
    } else if (task->state == 0) {
        uiFitPanelToList(list, &(obj)->panel);
        obj->panel.control.word = one;
        gGameSession->uiOpen    = one;
        uiSetListSystemCursorSound(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        uiDrawPanelLabel(&(obj)->panel, Telephone_Data_8017D638);
        uiUpdateList(list, &obj->panel);
    }
    if (obj->result == USER_INTERFACE_RESULT_CONFIRM) {
        obj->result = USER_INTERFACE_RESULT_NONE;
        uiStartPanelHiding(obj, task);
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
        if (task->state != 0) {
            sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
        }
        gGameSession->uiOpen = 0;
        obj->result          = USER_INTERFACE_RESULT_CANCEL;
        obj->resultValue     = 0x34;
    }
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        sel      = childObj->result;
        switch (sel) {
            case USER_INTERFACE_RESULT_CONFIRM:
                if (task->state == 1) {
                    kind = childObj->resultValue;
                    uiStartTreeClosing(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else if (task->state == 3) {
                    obj->result      = USER_INTERFACE_RESULT_CANCEL;
                    obj->resultValue = 0x34;
                } else {
                    uiStartTreeClosing(childObj, childObj->owner);
                    sndEvtRequestScriptStart(SOUND_SYSTEM_CANCEL, 0, 0);
                    uiStartPanelOpening(&(obj)->panel, task);
                    obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                }
                break;
            case USER_INTERFACE_RESULT_CANCEL:
                if (task->state == 1) {
                    kind = childObj->resultValue;
                    uiStartTreeClosing(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else {
                    obj->result      = USER_INTERFACE_RESULT_CANCEL;
                    obj->resultValue = 0x34;
                }
                break;
        }
    }
}
