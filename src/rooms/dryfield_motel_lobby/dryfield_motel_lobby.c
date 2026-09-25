#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"

extern UiObject*    D_80067634;
extern UiObjectDesc D_800611E4;
extern UiObjectDesc D_8010EFA0;

/// Labels the four menu-entry handlers `func_dryfield_motel_lobby_8017F094` to
/// `func_dryfield_motel_lobby_8017F308` draw: "Save", "Play Data", "Weapon
/// Data" and "PE Data".
extern u8 D_dryfield_motel_lobby_8017F4F0[];
extern u8 D_dryfield_motel_lobby_8017F4F8[];
extern u8 D_dryfield_motel_lobby_8017F504[];
extern u8 D_dryfield_motel_lobby_8017F510[];

/// Row labels of the play-data statistics panel, one per row
/// `func_dryfield_motel_lobby_8017D650` draws.
extern u8 D_dryfield_motel_lobby_8017F518[];
extern u8 D_dryfield_motel_lobby_8017F548[];
extern u8 D_dryfield_motel_lobby_8017F520[];
extern u8 D_dryfield_motel_lobby_8017F524[];
extern u8 D_dryfield_motel_lobby_8017F52C[];
extern u8 D_dryfield_motel_lobby_8017F538[];
extern u8 D_dryfield_motel_lobby_8017F550[];
extern u8 D_dryfield_motel_lobby_8017F558[];
extern u8 D_dryfield_motel_lobby_8017F560[];

/// The " times" suffix appended to that panel's count rows.
extern u8 D_dryfield_motel_lobby_8017F568[];

/// The "%" suffix appended to the percentages the play-data panels print.
extern u8 D_dryfield_motel_lobby_8017F570[];

/// Help texts of the panel's nine rows, handed to the UI holder for the
/// selected row.
extern u8 D_dryfield_motel_lobby_8017F574[];
extern u8 D_dryfield_motel_lobby_8017F5A0[];
extern u8 D_dryfield_motel_lobby_8017F5C4[];
extern u8 D_dryfield_motel_lobby_8017F5F4[];
extern u8 D_dryfield_motel_lobby_8017F628[];
extern u8 D_dryfield_motel_lobby_8017F65C[];
extern u8 D_dryfield_motel_lobby_8017F694[];
extern u8 D_dryfield_motel_lobby_8017F6C8[];
extern u8 D_dryfield_motel_lobby_8017F700[];

/// The "Play Data" panel's row list.
extern UiList D_dryfield_motel_lobby_8017F73C;

/// The usage panel's row list.
extern UiList D_dryfield_motel_lobby_8017F764;

/// UI descriptor the "Play Data" panel and the usage panel spawn when they
/// first open.
extern UiObjectDesc D_dryfield_motel_lobby_8017F788;

/// UI descriptors the "Play Data" entry and the two usage entries open.
extern UiObjectDesc D_dryfield_motel_lobby_8017F7A4;
extern UiObjectDesc D_dryfield_motel_lobby_8017F7C0;

extern const char D_dryfield_motel_lobby_8017D638[];

/// The telephone menu's entry list.
extern UiList D_dryfield_motel_lobby_8017F7EC;

/// The room's message table, which `func_dryfield_motel_lobby_8017F44C`
/// installs on the room task.
extern GpMsgEntry D_dryfield_motel_lobby_8017F810[];

void func_dryfield_motel_lobby_8017E218(UiList* list, UiObject* obj);
void func_dryfield_motel_lobby_8017E514(UiList* list, UiObject* obj);
void func_dryfield_motel_lobby_8017F3D0(Task* task);
void func_dryfield_motel_lobby_8017F44C(Task* task);
void func_dryfield_motel_lobby_8017F490(Task* task);

/// Draws one row of the play-data statistics panel: the row label, then the
/// statistic `arg0->field_8` selects - play time, several save counters, and
/// two percentages printed with two decimals and a "%" suffix. While the row is
/// selected its help text goes to the UI holder.
void func_dryfield_motel_lobby_8017D650(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_dryfield_motel_lobby_8017F574,
                D_dryfield_motel_lobby_8017F5A0,
                D_dryfield_motel_lobby_8017F5C4,
                D_dryfield_motel_lobby_8017F5F4,
                D_dryfield_motel_lobby_8017F628,
                D_dryfield_motel_lobby_8017F65C,
                D_dryfield_motel_lobby_8017F694,
                D_dryfield_motel_lobby_8017F6C8,
                D_dryfield_motel_lobby_8017F700,
            };

            Ui_SetHolderParam((s32)tbl[arg0->field_8], 0, 0);
        }
    }

    switch (arg0->field_8) {
        case 0: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_motel_lobby_8017F518);
            Text_FormatTime(p, Mc_SaveData.playTime);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 1: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_motel_lobby_8017F548);
            Text_ItoaUnsigned(p, Mc_SaveData.saveCount);
            Text_Strcat(p, D_dryfield_motel_lobby_8017F568);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 2: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_motel_lobby_8017F520);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CC);
            Text_Strcat(p, D_dryfield_motel_lobby_8017F568);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 3: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_motel_lobby_8017F524);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CE);
            Text_Strcat(p, D_dryfield_motel_lobby_8017F568);
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

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_motel_lobby_8017F52C);
            if (Mc_SaveData.field_6CC == 0) {
                pct = 0;
            } else {
                pct = (Mc_SaveData.field_6CC * 10000) / (Mc_SaveData.field_6CC + Mc_SaveData.field_6CE);
            }
            if (pct < 100) {
                func_8002F44C(p, pct, 3);
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
            Text_Strcat(p, D_dryfield_motel_lobby_8017F570);
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

            total          = Mc_SaveData.field_6CC;
            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_motel_lobby_8017F538);
            cnt   = 326;
            total = total + (GameFlag_GetNibble(0x167) + GameFlag_GetNibble(0x168));
            if (total == 0) {
                pct = 0;
            } else {
                pct = (total * 10000) / cnt;
            }
            if (pct < 100) {
                func_8002F44C(p, pct, 3);
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
            Text_Strcat(p, D_dryfield_motel_lobby_8017F570);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            Ui_DrawHBar((UiPanel*)arg1, arg1->field_1C, (s16)arg1->field_1E, arg0->field_1A + 3);
            arg0->field_1A = (u16)arg0->field_1A + 5;
            break;
        }
        case 6: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_motel_lobby_8017F550);
            Text_ItoaUnsigned(p, Mc_SaveData.clearCount);
            Text_Strcat(p, D_dryfield_motel_lobby_8017F568);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 7: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_motel_lobby_8017F558);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData.field_92C), arg0->field_1C, 3, 2);
            break;
        }
        case 8: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_motel_lobby_8017F560);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the "Play Data" panel `func_dryfield_motel_lobby_8017EEA0` draws.
const char D_dryfield_motel_lobby_8017D610[] = "Play Data";

/// Text drawn in place of a row's percentage once it reaches 100 percent.
const u8 D_dryfield_motel_lobby_8017D61C[] = "100.0%";

/// Draws one row of the item-usage panel: the item name and icon, its share of
/// all uses as a percentage with two decimals, and a gouraud bar sized from the
/// row's bar width. While the row is selected the item is previewed, and a
/// confirm press opens the item's description.
void func_dryfield_motel_lobby_8017DE1C(DialogPrompt* arg0, UiObject* arg1)
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
    if (arg1->mode != 5) {
        req.x          = arg1->baseX + 0x11 + x;
        textY          = arg1->baseY - 6;
        req.y          = textY + y;
        req.otIndex    = (s16)arg1->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        func_8002E53C(r, (u8*)Gp_GetItemText(item, 0, 0));
        func_800CE5D0(arg1, x, y, item);
    }
    limit = 1;
    if (value >= 10000) {
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_dryfield_motel_lobby_8017D61C, arg0->field_1C, 3, 2);
    } else {
        for (i = 2; i > 0; i--) {
            limit *= 10;
        }
        if (value < limit) {
            func_8002F44C(p, value, 3);
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
        Text_Strcat(p, D_dryfield_motel_lobby_8017F570);
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
    }

    base  = (s16)arg1->field_1C + 0x80;
    avail = (s16)arg1->field_1E - 0x4A;
    barW  = avail - base;
    barW  = (barW * work->barWidths[arg0->field_8]) >> 12;
    rowY  = arg0->field_1A - 0xC;
    barW  = barW + 2;
    barX  = avail - barW;
    if (barW >= 2) {
        prim                     = (POLY_G4*)gGpuPrimCursor;
        tx                       = arg1->baseX + barX + 1;
        prim->x2                 = tx;
        prim->x0                 = tx;
        ty                       = arg1->baseY;
        gGpuPrimCursor           = prim + 1;
        ty                       = ty + rowY;
        ty                      += 1;
        PRIM_COLOR_WORD(prim, 3) = PRIM_RGBC(0, 0, 0x01, 0);
        PRIM_COLOR_WORD(prim, 1) = PRIM_RGBC(0, 0, 0x01, 0);
        setlen(prim, 8);
        PRIM_COLOR_WORD(prim, 0) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        setcode(prim, 0x38);
        PRIM_COLOR_WORD(prim, 2) = PRIM_RGBC(0xb0, 0, 0x01, 0);
        tx                       = (u16)prim->x0 + barW - 1;
        prim->y1                 = ty;
        prim->y0                 = ty;
        ty                      += 8;
        prim->y3                 = ty;
        prim->y2                 = ty;
        prim->x3                 = tx;
        prim->x1                 = tx;
        addPrim(gGpuCurrentOt + (s16)arg1->drawOrder + 1, prim);
    }
    one = 1;
    func_80046B34((UiPanel*)arg1, barX, arg0->field_1A - 0xC, barW, 9, 0, one);
    if (((arg1->status >> 16) == one) || (arg1->status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}

/// Fills the weapon-usage panel's rows from the save's per-weapon use counters
/// (`Mc_SaveData.weaponUseCounts`, item ids 0x80-0x9F).
///
/// Every id with a non-empty name (a leading 0 or 0xA marks an unused row) and
/// a non-zero counter is marked seen and appended to `itemIds`, and the
/// counters are summed. The ids are insertion-sorted by use count, most-used
/// first. Each row then gets `percents`, its share of all uses in hundredths of
/// a percent, rounded, and `barWidths`, its counter as a 12-bit fraction of the
/// top row's. The top counter, the total and the scale are halved (and the
/// shift shortened) until the top counter fits in 17 bits, so neither the
/// multiply nor the shift overflows.
void func_dryfield_motel_lobby_8017E218(UiList* list, UiObject* obj)
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
        if ((c != 0) && (c != 0xA) && (Mc_SaveData.weaponUseCounts[i] > 0)) {
            Gp_SetItemSeenBit(id, 1);
            *p++ = id;
            count++;
            total += Mc_SaveData.weaponUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            uses = Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80];
            for (j = 0; j < i; j++) {
                if (Mc_SaveData.weaponUseCounts[work->itemIds[j] - 0x80] < uses) {
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
        top   = Mc_SaveData.weaponUseCounts[work->itemIds[0] - 0x80];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            work->percents[i] =
                (u32)((Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80] * scale) / total + 1) >> 1;
            work->barWidths[i] =
                (Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80] << shift) / top;
        }
    }

    list->field_4  = count;
    list->field_9  = 0;
    list->field_10 = 0;
}

/// Fills the PE-usage panel's rows the same way from the save's per-slot
/// Parasite Energy use counters (`Mc_SaveData.attachUseCounts`, twelve slots).
///
/// Slot `i` owns three consecutive ids from `i * 3 + 0xF`, one per level, and
/// is listed under the id of its current level in `Mc_SaveData.attachLevels`
/// (the base id while that level is 0). The sort, percentages and bar widths
/// follow the weapon panel, with each row's slot recovered from its id.
void func_dryfield_motel_lobby_8017E514(UiList* list, UiObject* obj)
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
    /* Matching only. The original object frames 24 bytes it never touches and
     * materialises GCC's `/3` magic constant before the first loop for a use
     * that no longer survives, so `scratch` buys the frame size and `magic`
     * plus the (instruction-free) SOFT_USE_REG below buy the allocation. */
    s32 magic;
    s16 scratch[12];

    count = 0;
    total = 0;
    i     = 0;
    work  = (RoomPeUsage*)obj->owner->work;
    p     = work->peIds;
    magic = 0x55555556;

    for (; i < 12; i++) {
        if (Mc_SaveData.attachUseCounts[i] > 0) {
            id = i * 3 + 0xF;
            *p = id;
            if (Mc_SaveData.attachLevels[i] != 0) {
                *p = id + (Mc_SaveData.attachLevels[i] - 1u);
            }
            p++;
            count++;
            total += Mc_SaveData.attachUseCounts[i];
        }
    }
    SOFT_USE_REG(magic);

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            slot = (work->peIds[i] - 0xF) / 3;
            uses = Mc_SaveData.attachUseCounts[slot];
            for (j = 0; j < i; j++) {
                slot = (work->peIds[j] - 0xF) / 3;
                if (Mc_SaveData.attachUseCounts[slot] < uses) {
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
        top   = Mc_SaveData.attachUseCounts[slot];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            slot               = (work->peIds[i] - 0xF) / 3;
            work->percents[i]  = (u32)((Mc_SaveData.attachUseCounts[slot] * scale) / total + 1) >> 1;
            slot               = (work->peIds[i] - 0xF) / 3;
            work->barWidths[i] = (Mc_SaveData.attachUseCounts[slot] << shift) / top;
        }
    }

    list->field_4  = count;
    list->field_9  = 0;
    list->field_10 = 0;
}

/// Titles of the usage panel `func_dryfield_motel_lobby_8017E834` opens:
/// "Weapon Data" and "PE Data".
const char D_dryfield_motel_lobby_8017D624[] = "Weapon Data";
const char D_dryfield_motel_lobby_8017D630[] = "PE Data";

void func_dryfield_motel_lobby_8017E834(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_dryfield_motel_lobby_8017F764;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_dryfield_motel_lobby_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_dryfield_motel_lobby_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F788, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_dryfield_motel_lobby_8017E218(list, obj);
        } else {
            func_dryfield_motel_lobby_8017E514(list, obj);
        }
        Ui_InitList(list, (UiMiniObj*)obj);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2;
            next     = child->nextSibling;
            if (childObj->field_2E == -1 || childObj->field_2E == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->status = 1;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

/// The "Telephone" title `func_dryfield_motel_lobby_8017E9E8` draws over its
/// menu once the menu is open. The two bytes after its terminator are not
/// zero, so it stays assembly.
INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_lobby/dryfield_motel_lobby", D_dryfield_motel_lobby_8017D638);

void func_dryfield_motel_lobby_8017E9E8(Task* task)
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

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    ready         = Mc_SaveData.demoScene == 1;
    list          = &D_dryfield_motel_lobby_8017F7EC;
    one           = 1;
    if (Mc_SaveData.clearCount > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            gGameSession->uiOpen = one;
            Ui_SpawnFromDesc(&D_800611E4, 0, 0, 0, obj);
            obj->status   = 0;
            obj->field_4 |= 0x80000000;
            task->state   = task->state + 1;
        }
    } else if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->status          = one;
        gGameSession->uiOpen = one;
        Ui_SetListScrollFlag(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        Ui_DrawText((UiPanel*)obj, D_dryfield_motel_lobby_8017D638);
        Ui_UpdateListNoAnim(list, obj);
    }
    if (obj->field_2E == 6) {
        obj->field_2E = 0;
        Ui_SetState4((Task*)obj, task);
        obj->status = 0;
    }
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        if (task->state != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
        }
        gGameSession->uiOpen = 0;
        obj->field_2E        = -1;
        obj->field_2C        = 0x34;
    }
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
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
                    Ui_StartCloseAnim((UiPanel*)obj, task);
                    obj->status = 1;
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

void func_dryfield_motel_lobby_8017ECE0(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_dryfield_motel_lobby_8017F3D0;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into the digit string `str` so that `decimals` digits (at most
/// the string's length) follow it, shifting them and the NUL one byte right.
/// Does nothing when `decimals` is not positive.
void func_dryfield_motel_lobby_8017ED3C(u8* str, s32 decimals)
{
    s32 len;

    len = 0;
    if (decimals > 0) {
        if (*str != 0) {
            do {
                str += 1;
                len += 1;
            } while (*str != 0);
        }
        if (len < decimals) {
            decimals = len;
            SOFT_TOUCH_REG(decimals);
            decimals += 1;
        } else {
            decimals += 1;
        }
        len = 0;
        if (decimals > 0) {
            do {
                len   += 1;
                str[1] = str[0];
                str   -= 1;
            } while (len < decimals);
        }
        str[1] = '.';
    }
}

/// Formats `value`, a percentage scaled by 10^`decimals`, into `buf` and
/// returns `buf`: the integer is printed padded to `decimals + 1` digits when
/// it is below that scale, a '.' is inserted before its last `decimals` digits,
/// and "%" is appended.
u8* func_dryfield_motel_lobby_8017EDAC(u8* buf, s32 value, s32 decimals)
{
    s32 limit;
    s32 i;
    s32 len;
    s32 n;
    u8* p;

    limit = 1;
    for (i = decimals; i > 0; i--) {
        limit *= 10;
    }

    if (value < limit) {
        func_8002F44C(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    n   = decimals;
    p   = buf;
    len = 0;
    if (n > 0) {
        while (*p != 0) {
            p++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            p[1] = p[0];
            p--;
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_dryfield_motel_lobby_8017F570);
    return buf;
}

void func_dryfield_motel_lobby_8017EEA0(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_motel_lobby_8017F73C;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_dryfield_motel_lobby_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F788, 0, 0, 1, obj);
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->field_12 += 5;
        list->field_A  = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
}

/// Queues a gouraud rectangle into the current OT one slot past the panel's
/// draw order, at the panel origin (`field_20`, `field_22`) offset by (`arg1`,
/// `arg2`) and `arg3` by `arg4` in size. The left edge takes colour `arg5`, the
/// right edge `arg6`; nothing is drawn for a zero `arg5` or a width below 2.
void func_dryfield_motel_lobby_8017EF90(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    register s32 dx asm("v1");
    register s32 w asm("t3");
    POLY_G4*     prim;
    s16          x;
    s16          y;

    dx = arg1;
    w  = arg3;
    if ((arg5 != 0) && (w >= 2)) {
        prim           = (POLY_G4*)gGpuPrimCursor;
        x              = arg0->field_20 + dx + 1;
        prim->x2       = x;
        prim->x0       = x;
        y              = arg0->field_22;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 8);
        PRIM_COLOR_WORD(prim, 0) = arg5;
        setcode(prim, 0x38);
        PRIM_COLOR_WORD(prim, 2) = arg5;
        PRIM_COLOR_WORD(prim, 3) = arg6;
        PRIM_COLOR_WORD(prim, 1) = arg6;
        y                        = y + arg2 + 1;
        x                        = (u16)prim->x0 + w - 1;
        prim->y1                 = y;
        prim->y0                 = y;
        prim->x3                 = x;
        prim->x1                 = x;
        y                        = y + arg4 - 1;
        prim->y3                 = y;
        prim->y2                 = y;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 1, prim);
    }
}

void func_dryfield_motel_lobby_8017F094(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F4F0, prompt->field_1C, 1, 0);
    sel = prompt->field_C;
    if (sel == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0 && CdCmd_IsIdle() != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        gDisplayState.gameMode = 0xFF;
        Ui_SpawnFromDesc(&D_800611E4, 1, 0, 0, obj);
        obj->status       = 0;
        obj->field_2E     = 6;
        obj->owner->state = sel;
    }
}

void func_dryfield_motel_lobby_8017F178(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F4F8, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F7A4, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_motel_lobby_8017F240(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F504, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F7C0, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_motel_lobby_8017F308(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_motel_lobby_8017F510, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_motel_lobby_8017F7C0, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_motel_lobby_8017F3D0(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

s32 func_dryfield_motel_lobby_8017F40C(void)
{
    return 0;
}

/// Location-message handler of the room's message table: copies the requested
/// location onto the outgoing record and answers 1.
s32 func_dryfield_motel_lobby_8017F414(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

s32 func_dryfield_motel_lobby_8017F43C(void)
{
    return 0;
}

s32 func_dryfield_motel_lobby_8017F444(void)
{
    return 0;
}

/// First state of the room task: installs the room's message table, stores the
/// task in session pointer slot 7 and advances.
void func_dryfield_motel_lobby_8017F44C(Task* task)
{
    task->msgTable = D_dryfield_motel_lobby_8017F810;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Per-frame state of the room task: the room has nothing to update.
void func_dryfield_motel_lobby_8017F490(Task* task)
{
}

/// The room task's three states: set-up, the (empty) per-frame state, and the
/// kill.
const TaskFuncTable3 D_dryfield_motel_lobby_8017D644 = {
    {
        func_dryfield_motel_lobby_8017F44C,
        func_dryfield_motel_lobby_8017F490,
        taskKill,
    },
};

/// The room task's update: copies the state table and runs the current state.
void func_dryfield_motel_lobby_8017F498(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_lobby_8017D644;
    sp.funcs[task->state](task);
}
