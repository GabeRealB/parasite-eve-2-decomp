#include "telephone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

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

#include "rooms/room_common.h"

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
/// `UiList::field_8`: a caption followed by a value - play time, one of
/// several counters with a unit suffix, or a percentage kept in hundredths
/// whose decimal point is inserted by hand (row 5 also draws a gauge and takes
/// an extra line). While the cursor is on the row its help string is shown.
static void Telephone_DrawPlayDataRow(UiList* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->panel.field_0.w >> 16) == 1) || (arg1->panel.field_0.w == 1)) {
        if (arg0->field_10 == arg0->field_8) {
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

            Ui_SetHolderParam(tbl[arg0->field_8], 0, 0);
        }
    }

    switch (arg0->field_8) {
        case 0: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Telephone_Data_80181A20);
            Text_FormatTime(p, Mc_SaveData[0].state.playTime);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 1: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Telephone_Data_80181A50);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.saveCount);
            Text_Strcat(p, Telephone_Data_80181A70);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 2: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Telephone_Data_80181A28);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CC);
            Text_Strcat(p, Telephone_Data_80181A70);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 3: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Telephone_Data_80181A2C);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_6CE);
            Text_Strcat(p, Telephone_Data_80181A70);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
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

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Telephone_Data_80181A34);
            if (Mc_SaveData[0].state.field_6CC == 0) {
                pct = 0;
            } else {
                pct = (Mc_SaveData[0].state.field_6CC * 10000) / (Mc_SaveData[0].state.field_6CC + Mc_SaveData[0].state.field_6CE);
            }
            if (pct < 100) {
                Text_ItoaPadded(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
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
            Text_Strcat(p, Telephone_Data_80181A78);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
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

            total          = Mc_SaveData[0].state.field_6CC;
            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Telephone_Data_80181A40);
            cnt   = 326;
            total = total + (GameFlag_GetNibble(0x167) + GameFlag_GetNibble(0x168));
            if (total == 0) {
                pct = 0;
            } else {
                pct = (total * 10000) / cnt;
            }
            if (pct < 100) {
                Text_ItoaPadded(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
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
            Text_Strcat(p, Telephone_Data_80181A78);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            Ui_DrawHBar(&(arg1)->panel, arg1->panel.field_1C.s, (s16)arg1->panel.field_1E.u, arg0->field_1A + 3);
            arg0->field_1A = (u16)arg0->field_1A + 5;
            break;
        }
        case 6: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Telephone_Data_80181A58);
            Text_ItoaUnsigned(p, Mc_SaveData[0].state.clearCount);
            Text_Strcat(p, Telephone_Data_80181A70);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 7: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Telephone_Data_80181A60);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_92C), arg0->field_1C, 3, 2);
            break;
        }
        case 8: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->panel.field_20.u + (u16)arg0->field_18;
            y              = arg1->panel.field_22.u - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = arg1->panel.field_14.s + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            Text_DrawString(&req, Telephone_Data_80181A68);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData[0].state.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

static const char Telephone_Data_8017D610[] = "Play Data";

static const u8 Telephone_Data_8017D61C[] = "100.0%";

/// Draws one row of an item-usage panel from the `RoomItemUsage` block in the
/// owning task's work area: the item's name, its share of all recorded uses as
/// a percentage with two decimals, and a gauge scaled by the row's
/// `barWidths` entry. Highlighting the row previews the item; pressing the
/// detail button on the selected row opens the item's detail window.
static void Telephone_DrawUsageRow(UiList* arg0, UiObject* arg1)
{
    u8             buf[0x20];
    TextDrawReq    req;
    TextDrawReq*   r;
    RoomItemUsage* work;
    POLY_G4*       prim;
    u8*            p;
    u8*            q;
    s32            item;
    s32            value;
    s32            x;
    s32            y;
    s32            color;
    s32            textY;
    s32            limit;
    s32            n;
    s32            len;
    s32            i;
    s32            avail;
    s32            base;
    s32            barW;
    s32            barX;
    s32            rowY;
    s32            one;
    s32            tx;
    s32            ty;

    p     = buf;
    r     = &req;
    x     = arg0->field_18;
    y     = arg0->field_1A;
    work  = (RoomItemUsage*)arg1->owner->work;
    item  = work->itemIds[arg0->field_8];
    value = work->percents[arg0->field_8];
    color = arg0->field_1C;
    if (arg1->panel.field_8 != 5) {
        req.x          = arg1->panel.field_20.u + 0x11 + x;
        textY          = arg1->panel.field_22.u - 6;
        req.y          = textY + y;
        req.otIndex    = arg1->panel.field_14.s + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        Text_DrawString(r, (u8*)Gp_GetItemText(item, 0, 0));
        func_800CE5D0(arg1, x, y, item);
    }
    limit = 1;
    if (value >= 10000) {
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Telephone_Data_8017D61C, arg0->field_1C, 3, 2);
    } else {
        for (i = 2; i > 0; i--) {
            limit *= 10;
        }
        if (value < limit) {
            Text_ItoaPadded(p, value, 3);
        } else {
            Text_ItoaUnsigned(p, value);
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
        Text_Strcat(p, Telephone_Data_80181A78);
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
    }

    base  = (s16)arg1->panel.field_1C.s + 0x80;
    avail = (s16)arg1->panel.field_1E.u - 0x4A;
    barW  = avail - base;
    barW  = (barW * work->barWidths[arg0->field_8]) >> 12;
    rowY  = arg0->field_1A - 0xC;
    barW  = barW + 2;
    barX  = avail - barW;
    if (barW >= 2) {
        prim                              = gGpuPrimCursor;
        tx                                = arg1->panel.field_20.u + barX + 1;
        prim->x2                          = tx;
        prim->x0                          = tx;
        ty                                = arg1->panel.field_22.u;
        gGpuPrimCursor                    = prim + 1;
        ty                                = ty + rowY;
        ty                               += 1;
        GPU_PRIMITIVE_COLOR_WORD(prim, 3) = PRIM_RGBC(0, 0, 0x01, 0);
        GPU_PRIMITIVE_COLOR_WORD(prim, 1) = PRIM_RGBC(0, 0, 0x01, 0);
        setlen(prim, 8);
        GPU_PRIMITIVE_COLOR_WORD(prim, 0) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        setcode(prim, 0x38);
        GPU_PRIMITIVE_COLOR_WORD(prim, 2) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        tx                                = (u16)prim->x0 + barW - 1;
        prim->y1                          = ty;
        prim->y0                          = ty;
        ty                               += 8;
        prim->y3                          = ty;
        prim->y2                          = ty;
        prim->x3                          = tx;
        prim->x1                          = tx;
        addPrim(gGpuCurrentOt + arg1->panel.field_14.s + 1, prim);
    }
    one = 1;
    Ui_DrawBeveledRect(&(arg1)->panel, barX, arg0->field_1A - 0xC, barW, 9, 0, one);
    if (((arg1->panel.field_0.w >> 16) == one) || (arg1->panel.field_0.w == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->panel.field_0.w = 0;
        }
    }
}

/// Builds the "Play Data" item-usage panel's three parallel arrays from the
/// save's per-item use counters (`Mc_SaveData[0].state.weaponUseCounts`, ids 0x80-0x9F).
///
/// Every id whose name is non-empty (a leading 0 or 0xA marks an unused row)
/// and whose counter is non-zero is marked seen and appended to `itemIds`,
/// while the counters are summed. The ids are then insertion-sorted by use
/// count, most-used first. Finally each row gets `percents` - its share of all
/// recorded uses in hundredths of a percent, rounded - and `barWidths`, its
/// counter as a 12-bit fraction of the top row's. Both are scaled down by
/// halving until the top counter fits in 17 bits, so the multiply and the
/// shift cannot overflow.
static void Telephone_BuildWeaponUsage(UiList* list, UiObject* obj)
{
    RoomItemUsage* work;
    s32            count;
    s32            total;
    s32            i;
    s32            j;
    s32            k;
    s32            id;
    s32            tmp;
    s32            uses;
    s32            scale;
    s32            top;
    s32            shift;
    s16*           p;
    u8             c;

    count = 0;
    total = 0;
    work  = (RoomItemUsage*)obj->owner->work;
    p     = work->itemIds;

    for (i = 0; i < 0x20; i++) {
        id = i + 0x80;
        c  = *Gp_GetItemText(id, 0, 1);
        if ((c != 0) && (c != 0xA) && (Mc_SaveData[0].state.weaponUseCounts[i] > 0)) {
            Gp_SetItemSeenBit(id, 1);
            *p++ = id;
            count++;
            total += Mc_SaveData[0].state.weaponUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            uses = Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80];
            for (j = 0; j < i; j++) {
                if (Mc_SaveData[0].state.weaponUseCounts[work->itemIds[j] - 0x80] < uses) {
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
        top   = Mc_SaveData[0].state.weaponUseCounts[work->itemIds[0] - 0x80];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            work->percents[i] =
                (u32)((Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80] * scale) / total + 1) >> 1;
            work->barWidths[i] =
                (Mc_SaveData[0].state.weaponUseCounts[work->itemIds[i] - 0x80] << shift) / top;
        }
    }

    list->field_4   = count;
    list->field_9.u = 0;
    list->field_10  = 0;
}

/// Parasite Energy counterpart of `Telephone_BuildWeaponUsage`: fills
/// the "Play Data" PE-usage panel's `RoomPeUsage` block from the save's
/// per-slot use counters.
///
/// Each of the twelve Parasite Energy slots owns three consecutive ids starting
/// at 0xF, one per level, so slot `i` at level `Mc_SaveData[0].state.attachLevels[i]`
/// prints as `i * 3 + 0xF + level - 1` (a slot the player has never levelled
/// keeps the base id). Every slot with a non-zero counter in
/// `Mc_SaveData[0].state.attachUseCounts` is appended and its counter summed. Levels
/// are addressed by page and column, with three slots per page. The ids are
/// then insertion-sorted by use count, most-used first, and each row gets
/// `percents`, its share of all recorded uses in hundredths of a percent, and
/// `barWidths`, its counter as a 12-bit fraction of the top row's. Both are
/// scaled down by halving until the top counter fits in 17 bits, so the
/// multiply and the shift cannot overflow.
static void Telephone_BuildPeUsage(UiList* list, UiObject* obj)
{
    RoomPeUsage* work;
    s16*         p;
    s32          count;
    s32          total;
    s32          i;
    s32          j;
    s32          k;
    s32          id;
    s32          slot;
    s32          uses;
    s32          scale;
    s32          shift;
    s32          top;
    s32          tmp;

    count = 0;
    total = 0;
    i     = 0;
    work  = (RoomPeUsage*)obj->owner->work;
    p     = work->peIds;

    for (; i < 12; i++) {
        s32 useCount;

        useCount = Mc_SaveData[0].state.attachUseCounts[i];
        id       = i * 3 + 0xF;
        if (useCount > 0) {
            s32 page;
            s32 column;

            page   = i / 3;
            column = i % 3;
            *p     = id;
            if (Mc_SaveData[0].state.attachLevels[column + page * 3] != 0) {
                *p = id + (Mc_SaveData[0].state.attachLevels[column + page * 3] - 1u);
            }
            p++;
            count++;
            total += Mc_SaveData[0].state.attachUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            slot = (work->peIds[i] - 0xF) / 3;
            uses = Mc_SaveData[0].state.attachUseCounts[slot];
            for (j = 0; j < i; j++) {
                slot = (work->peIds[j] - 0xF) / 3;
                if (Mc_SaveData[0].state.attachUseCounts[slot] < uses) {
                    tmp = work->peIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->peIds[k + 1] = work->peIds[k];
                    }
                    work->peIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        slot  = (work->peIds[0] - 0xF) / 3;
        top   = Mc_SaveData[0].state.attachUseCounts[slot];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            slot               = (work->peIds[i] - 0xF) / 3;
            work->percents[i]  = (u32)((Mc_SaveData[0].state.attachUseCounts[slot] * scale) / total + 1) >> 1;
            slot               = (work->peIds[i] - 0xF) / 3;
            work->barWidths[i] = (Mc_SaveData[0].state.attachUseCounts[slot] << shift) / top;
        }
    }

    list->field_4   = count;
    list->field_9.u = 0;
    list->field_10  = 0;
}

static const char Telephone_Data_8017D624[] = "Weapon Data";

static const char Telephone_Data_8017D630[] = "PE Data";

/// Task body of the usage panel: `spawnArg1` 0 lists weapons, anything else
/// Parasite Energy. On its first frame it allocates the row block, spawns the
/// row descriptor and fills the list; every frame it updates the list, closes
/// on cancel, and tears down any child window that has finished.
static void Telephone_UsageTask(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    list          = &Telephone_Data_80181C6C;
    if (task->spawnArg1.value == 0) {
        Ui_DrawText(&(obj)->panel, Telephone_Data_8017D624);
    } else {
        Ui_DrawText(&(obj)->panel, Telephone_Data_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&Telephone_Data_80181C90, 0, 0, 1, obj);
        if (task->spawnArg1.value == 0) {
            Telephone_BuildWeaponUsage(list, obj);
        } else {
            Telephone_BuildPeUsage(list, obj);
        }
        Ui_InitList(list, &(obj)->panel);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2.pointer;
            next     = child->nextSibling;
            if (childObj->field_2E == -1 || childObj->field_2E == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->panel.field_0.w = 1;
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

    obj           = task->spawnArg2.pointer;
    obj->field_2E = 0;
    ready         = Mc_SaveData[0].state.demoScene == 1;
    list          = &Telephone_Data_80181CF4;
    one           = 1;
    if (Mc_SaveData[0].state.clearCount > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            gGameSession->uiOpen = one;
            Ui_SpawnFromDesc(&D_800611E4, 0, 0, 0, obj);
            obj->panel.field_0.w = 0;
            obj->panel.field_4  |= 0x80000000;
            task->state          = task->state + 1;
        }
    } else if (task->state == 0) {
        Ui_LayoutListPanel(list, &(obj)->panel);
        obj->panel.field_0.w = one;
        gGameSession->uiOpen = one;
        Ui_SetListScrollFlag(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        Ui_DrawText(&(obj)->panel, Telephone_Data_8017D638);
        Ui_UpdateListNoAnim(list, obj);
    }
    if (obj->field_2E == 6) {
        obj->field_2E = 0;
        Ui_SetState4(obj, task);
        obj->panel.field_0.w = 0;
    }
    if (obj->panel.field_0.w == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        if (task->state != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
        }
        gGameSession->uiOpen = 0;
        obj->field_2E        = -1;
        obj->field_2C        = 0x34;
    }
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2.pointer;
        sel      = childObj->field_2E;
        switch (sel) {
            case 6:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
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
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                } else {
                    Ui_TeardownTree(childObj, childObj->owner);
                    SndEvt_EnqueueType6(0x3B, 0, 0);
                    Ui_StartCloseAnim(&(obj)->panel, task);
                    obj->panel.field_0.w = 1;
                }
                break;
            case -1:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
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
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                }
                break;
        }
    }
}
