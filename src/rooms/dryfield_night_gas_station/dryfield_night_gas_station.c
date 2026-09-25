#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "rooms/dryfield_night_gas_station.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

extern UiObjectDesc D_800611E4;
extern UiObject*    D_80067634;
extern u8           D_80071086;
extern u16          D_80072174;
extern s8           D_80072176;
extern s8           D_8007272D;
extern u16          D_80072834;
extern u16          D_80072836;
extern u8           D_80072A93;
extern s32          D_80072A94;
extern s32          D_80072A98;
extern UiObjectDesc D_8010EFA0;
extern s8           D_80114C12;
extern u8           D_80115598;
extern s32          D_8011572C;
extern s32          D_80115750;
extern s32          D_80115758;
extern u8           D_801156F9;

void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Prompt texts: "Save", "Play Data", "Weapon Data" and "PE Data".
extern u8 D_dryfield_night_gas_station_80183D14[];
extern u8 D_dryfield_night_gas_station_80183D1C[];
extern u8 D_dryfield_night_gas_station_80183D28[];
extern u8 D_dryfield_night_gas_station_80183D34[];

/// Row labels of the "Play Data" statistics list, one per row index.
extern u8 D_dryfield_night_gas_station_80183D3C[];
extern u8 D_dryfield_night_gas_station_80183D6C[];
extern u8 D_dryfield_night_gas_station_80183D44[];
extern u8 D_dryfield_night_gas_station_80183D48[];
extern u8 D_dryfield_night_gas_station_80183D50[];
extern u8 D_dryfield_night_gas_station_80183D5C[];
extern u8 D_dryfield_night_gas_station_80183D74[];
extern u8 D_dryfield_night_gas_station_80183D7C[];
extern u8 D_dryfield_night_gas_station_80183D84[];

/// The " times" suffix appended to the statistics rows that count events.
extern u8 D_dryfield_night_gas_station_80183D8C[];

/// The "%" suffix appended to a formatted percentage.
extern u8 D_dryfield_night_gas_station_80183D94[];

/// Help lines shown for the selected statistics row, one per row index.
extern u8 D_dryfield_night_gas_station_80183D98[];
extern u8 D_dryfield_night_gas_station_80183DC4[];
extern u8 D_dryfield_night_gas_station_80183DE8[];
extern u8 D_dryfield_night_gas_station_80183E18[];
extern u8 D_dryfield_night_gas_station_80183E4C[];
extern u8 D_dryfield_night_gas_station_80183E80[];
extern u8 D_dryfield_night_gas_station_80183EB8[];
extern u8 D_dryfield_night_gas_station_80183EEC[];
extern u8 D_dryfield_night_gas_station_80183F24[];

/// The "Play Data" statistics list.
extern UiList D_dryfield_night_gas_station_80183F60;

/// The usage list shown by `func_dryfield_night_gas_station_8017E844`.
extern UiList D_dryfield_night_gas_station_80183F88;

/// UI descriptors the "Play Data" and usage prompts open.
extern UiObjectDesc D_dryfield_night_gas_station_80183FC8;
extern UiObjectDesc D_dryfield_night_gas_station_80183FE4;

/// The list shown by `func_dryfield_night_gas_station_8017E9F8`.
extern UiList D_dryfield_night_gas_station_80184010;

extern s32      D_dryfield_night_gas_station_80184034;
extern TaskDesc D_dryfield_night_gas_station_8018406C[];
extern s32      D_dryfield_night_gas_station_80184098;
extern s32      D_dryfield_night_gas_station_801840AC;
extern s32      D_dryfield_night_gas_station_801841FC;

/// The layout template and the live copy that
/// `func_dryfield_night_gas_station_8017FBD4` restores from it.
extern GpGridParams D_dryfield_night_gas_station_80184374;
extern GpGridParams D_dryfield_night_gas_station_8018ABBC;

extern SVECTOR D_dryfield_night_gas_station_80188580[];
extern s32     D_dryfield_night_gas_station_80188B0C;
extern s32     D_dryfield_night_gas_station_80188B64;
extern s32     D_dryfield_night_gas_station_80188BF4;
extern s32     D_dryfield_night_gas_station_80189014;
extern s32     D_dryfield_night_gas_station_8018920C;
extern s32     D_dryfield_night_gas_station_801892E4;
extern s32     D_dryfield_night_gas_station_80189A7C;

/// The room's effect anchors, 8 bytes apart. Entries 0-9 are drawn in pairs by
/// `func_dryfield_night_gas_station_801812B4`, 10-18 one at a time by
/// `func_dryfield_night_gas_station_80181AF8`, and 19-20 are where the spawned
/// effects are scattered around.
extern SVECTOR D_dryfield_night_gas_station_80189C8C[];

/// Entries 21-24 of the anchor list, reached by name: two
/// `func_dryfield_night_gas_station_801812B4` pairs drawn together whenever
/// one of views 2, 3, 13 or 14 is current.
extern SVECTOR D_dryfield_night_gas_station_80189D34[];

/// Per-anchor view masks, one word per anchor: bit `n` set draws the anchor
/// while view `n` is current.
extern s32 D_dryfield_night_gas_station_80189D54[];

/// The beam's two end points relative to the effect's parent coordinate;
/// the second is also read by its own name.
extern SVECTOR D_dryfield_night_gas_station_80189DA0[];
extern SVECTOR D_dryfield_night_gas_station_80189DA8;

extern GpAreaApplyRec D_dryfield_night_gas_station_801907A0;

/// Handle of the task spawned from entry 2 of
/// `D_dryfield_night_gas_station_801888A0`, or NULL while none runs.
extern Task* D_dryfield_night_gas_station_801907AC;

void func_dryfield_night_gas_station_8017E228(UiList* list, UiObject* obj);
void func_dryfield_night_gas_station_8017E524(UiList* list, UiObject* obj);
void func_dryfield_night_gas_station_8017FBD4(s32 arg0);
void func_dryfield_night_gas_station_80180C20(void);
void func_dryfield_night_gas_station_80180D1C(void);
void func_dryfield_night_gas_station_80180DC8(s16 arg0);
void func_dryfield_night_gas_station_80182CD4(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);

/// Draws one row of the "Play Data" statistics list: the label for row
/// `arg0->field_8` and its value (play time, save count, battles won and
/// escaped, the two percentages, clear count, maximum EXP and BP).
/// While the row is the selected one it also posts that row's help line.
void func_dryfield_night_gas_station_8017D660(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_dryfield_night_gas_station_80183D98,
                D_dryfield_night_gas_station_80183DC4,
                D_dryfield_night_gas_station_80183DE8,
                D_dryfield_night_gas_station_80183E18,
                D_dryfield_night_gas_station_80183E4C,
                D_dryfield_night_gas_station_80183E80,
                D_dryfield_night_gas_station_80183EB8,
                D_dryfield_night_gas_station_80183EEC,
                D_dryfield_night_gas_station_80183F24,
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
            func_8002E53C(&req, D_dryfield_night_gas_station_80183D3C);
            Text_FormatTime(p, D_80072174);
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
            func_8002E53C(&req, D_dryfield_night_gas_station_80183D6C);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_dryfield_night_gas_station_80183D8C);
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
            func_8002E53C(&req, D_dryfield_night_gas_station_80183D44);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_dryfield_night_gas_station_80183D8C);
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
            func_8002E53C(&req, D_dryfield_night_gas_station_80183D48);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_dryfield_night_gas_station_80183D8C);
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
            func_8002E53C(&req, D_dryfield_night_gas_station_80183D50);
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
            Text_Strcat(p, D_dryfield_night_gas_station_80183D94);
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

            total          = D_80072834;
            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_dryfield_night_gas_station_80183D5C);
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
            Text_Strcat(p, D_dryfield_night_gas_station_80183D94);
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
            func_8002E53C(&req, D_dryfield_night_gas_station_80183D74);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_dryfield_night_gas_station_80183D8C);
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
            func_8002E53C(&req, D_dryfield_night_gas_station_80183D7C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A94), arg0->field_1C, 3, 2);
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
            func_8002E53C(&req, D_dryfield_night_gas_station_80183D84);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the "Play Data" panel.
const char D_dryfield_night_gas_station_8017D610[] = "Play Data";

/// Drawn in place of the percentage for a row holding every recorded use.
const u8 D_dryfield_night_gas_station_8017D61C[] = "100.0%";

/// Draws one row of an item or PE usage list: the entry's name and icon, its
/// share of all uses as a percentage with two decimals, and a gouraud bar of
/// the entry's width. The selected row previews the entry, and pad bit 0x10
/// on it opens the entry's detail panel.
void func_dryfield_night_gas_station_8017DE2C(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_dryfield_night_gas_station_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_dryfield_night_gas_station_80183D94);
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
        prim             = (POLY_G4*)gGpuPrimCursor;
        tx               = arg1->baseX + barX + 1;
        prim->x2         = tx;
        prim->x0         = tx;
        ty               = arg1->baseY;
        gGpuPrimCursor   = prim + 1;
        ty               = ty + rowY;
        ty              += 1;
        *(s32*)&prim->r3 = 0x10000;
        *(s32*)&prim->r1 = 0x10000;
        setlen(prim, 8);
        *(s32*)&prim->r0 = 0x100B0;
        setcode(prim, 0x38);
        *(s32*)&prim->r2 = 0x100B0;
        tx               = (u16)prim->x0 + barW - 1;
        prim->y1         = ty;
        prim->y0         = ty;
        ty              += 8;
        prim->y3         = ty;
        prim->y2         = ty;
        prim->x3         = tx;
        prim->x1         = tx;
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

/// Fills the weapon-usage list from the save's per-weapon use counters (ids
/// 0x80-0x9F): every named weapon with a non-zero count is marked seen and
/// listed, the list is sorted most-used first, and each row gets its share of
/// all uses in hundredths of a percent and a bar width relative to the top row.
void func_dryfield_night_gas_station_8017E228(UiList* list, UiObject* obj)
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

/// Fills the PE-usage list the same way from the save's per-slot Parasite
/// Energy use counters: each of the twelve slots lists under its base id
/// `i * 3 + 0xF` advanced by its current level, sorted most-used first, with
/// the same percentage and bar-width columns.
void func_dryfield_night_gas_station_8017E524(UiList* list, UiObject* obj)
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

/// Titles of the weapon and PE usage panels.
const char D_dryfield_night_gas_station_8017D624[] = "Weapon Data";
const char D_dryfield_night_gas_station_8017D630[] = "PE Data";

/// Task body of the weapon (`spawnArg1 == 0`) or PE usage panel: on its first
/// tick it allocates the list's work block and fills it, then updates the
/// list each tick, closes on cancel and tears down children that finished.
void func_dryfield_night_gas_station_8017E844(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_dryfield_night_gas_station_80183F88;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_gas_station_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_gas_station_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_dryfield_night_gas_station_80183FAC, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_dryfield_night_gas_station_8017E228(list, obj);
        } else {
            func_dryfield_night_gas_station_8017E524(list, obj);
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

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", D_dryfield_night_gas_station_8017D638);

/// Task body of the "Telephone" menu: shows its list once the save has a
/// clear or the demo scene flag set (otherwise it spawns the fallback panel),
/// and turns a child's selection into an item prompt.
void func_dryfield_night_gas_station_8017E9F8(Task* task)
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
    list          = &D_dryfield_night_gas_station_80184010;
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
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_gas_station_8017D638);
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

/// Task body that draws a prompt's lines, taking `Wip_UiHolder` and
/// installing the exit callback on its first tick.
void func_dryfield_night_gas_station_8017ECF0(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_dryfield_night_gas_station_8017F3E0;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so that at most `decimals` digits
/// follow it; does nothing when `decimals` is not positive.
void func_dryfield_night_gas_station_8017ED4C(u8* str, s32 decimals)
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

/// Formats `value` into `buf` as a percentage with `decimals` fractional
/// digits: zero-padded to `decimals + 1` digits when smaller than
/// 10^`decimals`, with a '.' inserted before the last `decimals` digits and
/// "%" appended. Returns `buf`.
u8* func_dryfield_night_gas_station_8017EDBC(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_dryfield_night_gas_station_80183D94);
    return buf;
}

/// Task body of the "Play Data" panel: lays out its statistics list on the
/// first tick, then updates it each tick and closes on cancel.
void func_dryfield_night_gas_station_8017EEB0(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_night_gas_station_80183F60;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_dryfield_night_gas_station_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_night_gas_station_80183FAC, 0, 0, 1, obj);
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

/// Queues a gouraud rectangle one OT slot past the panel's draw order, at
/// (`arg1`, `arg2`) from the panel origin and `arg3` by `arg4` in size; the
/// left edge takes colour `arg5` and the right `arg6`. Nothing is drawn for a
/// zero `arg5` or a width below 2.
void func_dryfield_night_gas_station_8017EFA0(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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
        *(u32*)&prim->r0 = arg5;
        setcode(prim, 0x38);
        *(u32*)&prim->r2 = arg5;
        *(u32*)&prim->r3 = arg6;
        *(u32*)&prim->r1 = arg6;
        y                = y + arg2 + 1;
        x                = (u16)prim->x0 + w - 1;
        prim->y1         = y;
        prim->y0         = y;
        prim->x3         = x;
        prim->x1         = x;
        y                = y + arg4 - 1;
        prim->y3         = y;
        prim->y2         = y;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 1, prim);
    }
}

/// Prompt row "Save": on confirm, once the CD queue is idle, opens the save
/// panel and moves the owning task to state 1.
void func_dryfield_night_gas_station_8017F0A4(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_gas_station_80183D14, prompt->field_1C, 1, 0);
    sel = prompt->field_C;
    if (sel == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0 && CdCmd_IsIdle() != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        D_80071086 = 0xFF;
        Ui_SpawnFromDesc(&D_800611E4, 1, 0, 0, obj);
        obj->status       = 0;
        obj->field_2E     = 6;
        obj->owner->state = sel;
    }
}

/// Prompt row "Play Data": on confirm opens the statistics panel.
void func_dryfield_night_gas_station_8017F188(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_gas_station_80183D1C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_gas_station_80183FC8, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Prompt row "Weapon Data": on confirm opens the usage panel for weapons.
void func_dryfield_night_gas_station_8017F250(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_gas_station_80183D28, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_gas_station_80183FE4, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Prompt row "PE Data": on confirm opens the usage panel for Parasite
/// Energy.
void func_dryfield_night_gas_station_8017F318(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_gas_station_80183D34, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_gas_station_80183FE4, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_night_gas_station_8017F3E0(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

void func_dryfield_night_gas_station_8017F41C(Task* arg0)
{
    arg0->msgTable = &D_dryfield_night_gas_station_80184034;
    Game_SetPtrSlot(arg0, 7);
    if ((GameFlag_GetNibble(0x63) >= 2) && (gameGetPtrSlot(0xA) != 0)) {
        Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_night_gas_station_80188B0C, 0);
        Gp_AllyAnimId(&D_dryfield_night_gas_station_80184098);
        Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_night_gas_station_80184098, 0);
        func_dryfield_night_gas_station_8017FBD4(0);
    }
    if (GameFlag_GetNibble(0xA0) == 0) {
        GameFlag_SetNibble(0xA0, 1);
        func_800E3FAC(0xA2, 0x12);
        GameFlag_SetNibble(0x4C, 2);
        func_dryfield_night_gas_station_80180C20();
        if (gameGetPtrSlot(0xA) != 0) {
            func_800E8634((s32)&D_dryfield_night_gas_station_801892E4, 0, (s32)&D_dryfield_night_gas_station_80189A7C);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// Answers the room message `in`, copying it to `out` first. For message 2 it reports in `out->field_3` how far nibble 0x61 has
/// advanced (3 once nibble 0x7A reaches 4). Message 3 returns 2 when the
/// session sits at stage 3, place 1 with `Gp_StateF0` agreeing, and 0 while
/// nibble 0x3B is clear; message 2 returns 0 while nibble 0x45 reads 1. The
/// cap commands and nibble write that go with those answers run only when
/// `in->field_5` is clear. Every other case returns 1.
s32 func_dryfield_night_gas_station_8017F544(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 n;
    s32 val;

    *out = *in;
    if (in->msgId == 2 && in->field_5 == 0) {
        n = GameFlag_GetNibble(0x7A);
        if (n < 4) {
            val = 3;
            TOUCH_REG(val);
            val = GameFlag_GetNibble(0x61) + 1;
        } else {
            val = 3;
        }
        out->field_3 = val;
    }
    if (in->msgId == 3) {
        if ((gGameSession->at4.loc.stage == in->msgId) && (gGameSession->at4.loc.place == 1) &&
            (Gp_StateF0.field_0 == gGameSession->at4.loc.place)) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(0x15);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x3B) == 0) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(7);
                Gp_SetNibbleIf(in->field_6, 2);
            }
            return 0;
        }
    }
    if (in->msgId == 2) {
        if (GameFlag_GetNibble(0x45) == 1) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 0;
        }
    }
    return 1;
}

/// Maps a cap (cutscene) script event key to the stage sound it should play in
/// the night gas station, then enqueues it as a type-6 sound event. Event key
/// 0x83 only plays if a cap script is still reporting an event key. Keys with
/// no sound are ignored. Always returns 0.
s32 func_dryfield_night_gas_station_8017F6B8(s32 arg0, s32 arg1, s32 arg2)
{
    s32 id;

    switch (arg2) {
        case 5:
            id = 0x52010005;
            goto play;
        case 7:
            id = 0x52010007;
            goto play;
        case 0xA:
            id = 0x5201000A;
            goto play;
        case 0xD:
            id = 0x5201000D;
            goto play;
        case 0x11:
            id = 0x52010011;
            goto play;
        case 0x13:
            id = 0x52010013;
            goto play;
        case 0x6D:
        case 0x82:
            id = 0x5201000B;
            goto play;
        case 0x73:
            id = 0x5201000E;
            goto play;
        case 0x83:
            if (Gp_GetCapEventKey() == 0) {
                break;
            }
            id = 0x52010012;
        play:
            Gp_EnqueueStageSnd6(id, 0, 0);
            break;
    }
    return 0;
}

/// Message handler for msg 0x117: walks the `Gp_PendingObj4C` list looking for
/// an object in mode 5 whose `field_48` is 0xFF and which is still pending, and
/// on a hit flips `gGameSession->eventState` / `field_68` and spawns the night gas
/// station cutscene task. Answers 1 only when it found one.
s32 func_dryfield_night_gas_station_8017F7E0(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x117) {
        found = 0;
        node  = Gp_PendingObj4C;
        while (node != NULL) {
            if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                found = 1;
                break;
            }
            node  = node->next;
            found = 0;
        }

        if (found != 0) {
            gGameSession->eventState = 1;
            gGameSession->hideHud    = 1;
            Task_SpawnOnDefaultList(D_dryfield_night_gas_station_8018406C, 0, 0, 0);
            return 1;
        }
    }
    return 0;
}

s32 func_dryfield_night_gas_station_8017F89C(s32 arg0, s32 arg1, s32 arg2)
{
    s16 var_a2;

    if (arg2 == 1) {
        Gp_RunCapCmd1(0x11);
    }
    if (arg2 == 5) {
        if (Gp_HasCollectedBit(0x118) == 0) {
            Gp_HasCollectedBit(0x117);
            var_a2 = 0;
        } else {
            var_a2 = 1;
        }
        Gp_StartCapSlot(0x12, 1, var_a2);
    }
    if ((arg2 == 0x17) && (gGameSession->at4.loc.room == 4)) {
        if (Gp_HasCollectedBit(0x11E) != 0) {
            if (GameFlag_GetNibble(0xD4) == 0) {
                GameFlag_SetNibble(0xD4, 1);
            } else {
                GameFlag_SetNibble(0xD4, 2);
            }
        }
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xD4) != 0 ? (GameFlag_GetNibble(0xD4) == 1 ? 0x20 : 0x1F) : arg2, 0);
    }
    return 0;
}

/// Handler for slot-7 msg `0x13EF` in `D_dryfield_night_gas_station_80184034`:
/// the directed action selected by `field_2` 0xE runs the room's cutscene script
/// blob at `D_dryfield_night_gas_station_8018920C`, but only once nibble 0x63 has
/// reached 2 and pointer slot 0xA is live.
s32 func_dryfield_night_gas_station_8017F990(Task* task, s32 msgId, GpMsg13EF* msg)
{
    if ((msg->field_2 == 0xE) && (gameGetPtrSlot(0xA) != NULL) && (GameFlag_GetNibble(0x63) >= 2)) {
        func_800E8614((s32)&D_dryfield_night_gas_station_8018920C, 0);
    }
    return 0;
}

/// Arms the room's night sequence, once: while nibble 0x63 is still clear it
/// sets that nibble, plays the script blob at
/// `D_dryfield_night_gas_station_80188B64`, raises `gGameSession->flowFlags`
/// bit 0x80, applies the room's area records, clears nibbles 0x62 and 0x45 and
/// queues sound event 0x64.
s32 func_dryfield_night_gas_station_8017F9E8(void)
{
    if (GameFlag_GetNibble(0x63) == 0) {
        GameFlag_SetNibble(0x63, 1);
        func_800E8614((s32)&D_dryfield_night_gas_station_80188B64, 1);
        gGameSession->flowFlags |= 0x80;
        Gp_ApplyAreaRecs(&D_dryfield_night_gas_station_801907A0);
        GameFlag_SetNibble(0x62, 0);
        GameFlag_SetNibble(0x45, 0);
        SndEvt_EnqueueType2(0, 0x64);
    }
    return 0;
}

/// Tears the room's scripted sequence down: raises `gGameSession->hideHud`
/// and `D_80115768`, hides the display, clears collection bit 0x117, installs
/// the room's two cap files, runs the 0xA2/0x16 event and kills its own task.
void func_dryfield_night_gas_station_8017FA6C(Task* arg0)
{
    gGameSession->hideHud = 1;
    D_80115768            = 1;
    SetDispMask(0);
    Gp_ClearCollectedBit(0x117);
    func_800E8634((s32)&D_dryfield_night_gas_station_801840AC, 0, (s32)&D_dryfield_night_gas_station_801841FC);
    func_800E3FAC(0xA2, 0x16);
    D_8007272D = 4;
    taskKill(arg0);
}

/// Runs the room's one-shot post-sequence event: with the session still on its
/// first mode and nibble 0x63 reading 1 — and the cutscene flag agreeing — it
/// advances the nibble to 2 and plays the cap pair
/// `D_dryfield_night_gas_station_80188BF4` / `_80189014`.
void func_dryfield_night_gas_station_8017FAEC(Task* task)
{
    s32 temp_v0;

    if (gGameSession->eventState == 0) {
        temp_v0 = GameFlag_GetNibble(0x63);
        if ((temp_v0 == 1) && (D_80114C12 != temp_v0)) {
            GameFlag_SetNibble(0x63, 2);
            func_800E8634((s32)&D_dryfield_night_gas_station_80188BF4, 0, (s32)&D_dryfield_night_gas_station_80189014);
        }
    }
}

/// Stores `arg0` in `D_80115768`.
void func_dryfield_night_gas_station_8017FB64(u8 arg0)
{
    D_80115768 = arg0;
}

/// The three states of the room's main task, run by
/// `func_dryfield_night_gas_station_8017FB70`: set-up, the per-frame handler
/// and the kill.
const TaskFuncTable3 D_dryfield_night_gas_station_8017D644 = {
    {
        func_dryfield_night_gas_station_8017F41C,
        func_dryfield_night_gas_station_8017FAEC,
        taskKill,
    },
};

/// Offset added to a marker's position before it is projected.
const SVECTOR D_dryfield_night_gas_station_8017D650 = { 0x3B23, -0x498, -0xD76, 0 };

/// The lamp beam's direction vector in the lamp's model space.
const SVECTOR D_dryfield_night_gas_station_8017D658 = { -0x1E, 0x122, 0x28, 0 };

/// Gates the room's two sprite records on nibble 0x8D, then dispatches the task
/// through the room's own three-state table, copied onto the stack first.
void func_dryfield_night_gas_station_8017FB70(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_gas_station_8017D644;
    func_dryfield_night_gas_station_80180D1C();
    sp.funcs[arg0->state](arg0);
}

/// Resets the live layout lists from the template: the four-entry vector list
/// and its 12-byte records, then the eight-entry list, which is afterwards
/// raised by 0xBB8 on y when `arg0` is nonzero.
void func_dryfield_night_gas_station_8017FBD4(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_dryfield_night_gas_station_8018ABBC;
    src = &D_dryfield_night_gas_station_80184374;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0xBB8;
    }
    d.vz = 0;

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

/// Draws a marker for entry `arg0` of `D_dryfield_night_gas_station_80188580`:
/// the vector is turned by a fixed -0x262 yaw, offset by
/// `D_dryfield_night_gas_station_8017D650` and projected through
/// `Gfx_ViewWorldMtx`. When the projection passes, a semi-transparent 3x3 dark
/// red `TILE` with its draw-mode `DR_TPAGE` and a bright red `TILE_1` mark the
/// point. The point is then projected again and once more displaced by
/// (-0x3E8, +0x1F4, +0x1F4), and a semi-transparent `LINE_G2` runs from the
/// displaced point's x, 10 pixels below the point, back to the point, shading
/// from black to a red that flickers with `rand()`. Everything is linked into
/// OT slot 0xA.
void func_dryfield_night_gas_station_8017FD80(s32 arg0)
{
    SVECTOR     off;
    OverlayMat  mtx;
    SVECTOR     pos;
    s32         sxy;
    s32         p;
    s32         flag;
    u16         x0;
    u16         y0;
    u16         x1;
    u16         y1;
    u16         x2;
    SVECTOR*    vec;
    s32         val;
    s32         one;
    OverlayMat* m;
    TILE*       tile;
    TILE_1*     tile1;
    LINE_G2*    line;
    DR_TPAGE*   dr;

    off               = D_dryfield_night_gas_station_8017D650;
    one               = ONE;
    m                 = &mtx;
    mtx.ident.m00_m01 = one;
    mtx.ident.m02_m10 = 0;
    m->ident.m11_m12  = one;
    mtx.ident.m20_m21 = 0;
    m->ident.m22      = one;
    func_8004BFF8(-0x262, &mtx.mat);
    vec = &D_dryfield_night_gas_station_80188580[arg0];
    ApplyMatrixSV(&mtx.mat, vec, &pos);
    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    pos.vx += off.vx;
    pos.vy += off.vy;
    pos.vz += off.vz;
    RotTransPers(&pos, &sxy, &p, &flag);
    if (flag >= 0) {
        val            = 0xA;
        x0             = sxy;
        y0             = sxy >> 16;
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        setTile(tile);
        setSemiTrans(tile, 1);
        tile->x0 = x0 - 1;
        tile->y0 = y0 - 1;
        setRGB0(tile, 0x80, 0, 0);
        tile->w = 3;
        tile->h = 3;
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), tile);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), dr);
        tile1          = (TILE_1*)gGpuPrimCursor;
        gGpuPrimCursor = tile1 + 1;
        setTile1(tile1);
        setSemiTrans(tile1, 1);
        tile1->x0 = x0;
        tile1->y0 = y0;
        setRGB0(tile1, 0xFF, 0, 0);
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), tile1);

        mtx.ident.m00_m01 = one;
        mtx.ident.m02_m10 = 0;
        m->ident.m11_m12  = one;
        mtx.ident.m20_m21 = 0;
        m->ident.m22      = one;
        val               = -0x262;
        func_8004BFF8(val, &mtx.mat);
        ApplyMatrixSV(&mtx.mat, vec, &pos);
        SetRotMatrix(&Gfx_ViewWorldMtx);
        SetTransMatrix(&Gfx_ViewWorldMtx);
        pos.vx += off.vx;
        pos.vy += off.vy;
        pos.vz += off.vz;
        RotTransPers(&pos, &sxy, &p, &flag);
        x1      = sxy;
        y1      = sxy >> 16;
        pos.vx -= 0x3E8;
        pos.vy += 0x1F4;
        pos.vz += 0x1F4;
        RotTransPers(&pos, &sxy, &p, &flag);
        x2             = sxy;
        line           = (LINE_G2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineG2(line);
        setSemiTrans(line, 1);
        line->x0 = x2;
        line->y0 = y1 + 0xA;
        line->x1 = x1;
        line->y1 = y1;
        setRGB1(line, rand() % 60 + 0x50, 0, 0);
        val = 0xA;
        setRGB0(line, 0, 0, 0);
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), line);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim((u_long*)(((((u32)val << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), dr);
    }
}

/// Draws the room's lamp beam. The matrix comes from the slot-0xA model's
/// per-part coordinate array, 0x280 (part 8) in, composed into an identity
/// rotation on the stack; the beacon vector `D_dryfield_night_gas_station_8017D658`
/// is rotated by it twice - as-is and 0x190 further up - and both points are
/// added to that world position and projected through `Gfx_ViewWorldMtx`. Once
/// both `RotTransPers` FLAG words pass, a semi-transparent `LINE_G2` between
/// them, whose red channel flickers with `rand() % 100 - 0x7E`, and its
/// draw-mode `DR_TPAGE` are linked into OT slot 0xA.
void func_dryfield_night_gas_station_801802EC(s32 arg0)
{
    MATRIX    mtx;
    SVECTOR   pos;
    SVECTOR   p0;
    SVECTOR   p1;
    SVECTOR   off;
    s32       sxy;
    s32       p;
    s32       flag0;
    s32       flag1;
    u16       x0;
    u16       y0;
    u16       x1;
    u16       y1;
    s32       one;
    MATRIX*   m;
    GpCoord*  coord;
    LINE_G2*  line;
    DR_TPAGE* dr;

    off                = D_dryfield_night_gas_station_8017D658;
    one                = ONE;
    m                  = &mtx;
    *(s32*)&mtx        = one;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[1][1] = one;
    *(s32*)&m->m[2][0] = 0;
    m->m[2][2]         = one;
    coord              = gameGetPtrSlot(0xA)->extra.tmd->coords;
    Gp_ComposeParentWorld(&coord[8], m, &pos);
    ApplyMatrixSV(&mtx, &off, &p0);
    p0.vx  += pos.vx;
    p0.vy  += pos.vy;
    p0.vz  += pos.vz;
    off.vy += 0x190;
    ApplyMatrixSV(&mtx, &off, &p1);
    p1.vx += pos.vx;
    p1.vy += pos.vy;
    p1.vz += pos.vz;
    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    RotTransPers(&p0, &sxy, &p, &flag0);
    x0 = sxy;
    y0 = sxy >> 16;
    RotTransPers(&p1, &sxy, &p, &flag1);
    x1 = sxy;
    y1 = sxy >> 16;
    if (flag0 >= 0 && flag1 >= 0) {
        line           = (LINE_G2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineG2(line);
        setSemiTrans(line, 1);
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        setRGB0(line, rand() % 100 - 0x7E, 0, 0);
        setRGB1(line, 0, 0, 0);
        addPrim((u_long*)(((((u32)0xA << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), line);
        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 1, 0, 0x25);
        addPrim((u_long*)(((((u32)0xA << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), dr);
    }
}

/// Bursts the room's lamp effect: `arg0` picks one of three spawn-argument
/// triples and the effect's `arg2`, the spark is spawned at the lamp task's own
/// coordinate, and the lamp is then told to light up. Any other `arg0` only
/// switches the lamp back to dark.
void func_dryfield_night_gas_station_80180604(s32 arg0)
{
    GpWorkObj* work;
    GpCoord*   coord;
    SVECTOR    offset;

    work = Gp_FindWorkById(gGameSession->at4.loc.area | ((gGameSession->at4.loc.stage << 8) | 0x2000));
    if (work != NULL) {
        coord = ((Task*)work->field_0)->extra.tmd->coords;
        switch (arg0) {
            case 0:
                offset.vx = 0;
                offset.vy = -0x64;
                offset.vz = -0x12C;
                Gp_SpawnEff(0x600E0, coord, 0x300, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            case 1:
                offset.vx = 0xC8;
                offset.vy = -0x64;
                offset.vz = -0xC8;
                Gp_SpawnEff(0x600E0, coord, 0x200, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            case 2:
                offset.vx = -0x64;
                offset.vy = -0x64;
                offset.vz = -0xC8;
                Gp_SpawnEff(0x600E0, coord, 0x200, &offset);
                func_dryfield_night_gas_station_80180DC8(1);
                break;

            default:
                func_dryfield_night_gas_station_80180DC8(0);
                break;
        }
    }
}

void func_dryfield_night_gas_station_80180720(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_dryfield_night_gas_station_80180740(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_dryfield_night_gas_station_80180760(void)
{
    Gp_RestoreStreamRng();
}

void func_dryfield_night_gas_station_80180780(void)
{
    CdCmd_CancelReplaceAndActivate();
}

/// Spawns entry 0 of the room's task table and tracks it in
/// `D_dryfield_night_gas_station_801907A4`.
void func_dryfield_night_gas_station_801807A0(void)
{
    D_dryfield_night_gas_station_801907A4 = Task_SpawnFromTable(&D_dryfield_night_gas_station_801888A0, 0, 0, 0);
}

/// Passes `arg0` to the task tracked in `D_dryfield_night_gas_station_801907A4`
/// as its `spawnArg1` when it is 0 or 1; any other value kills the task and
/// clears the handle.
void func_dryfield_night_gas_station_801807D4(s32 arg0)
{
    Task* t = D_dryfield_night_gas_station_801907A4;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    taskKill(D_dryfield_night_gas_station_801907A4);
    D_dryfield_night_gas_station_801907A4 = NULL;
}

/// The room's tracked-task timer, run once a frame while `D_801156F9` is
/// clear. The slot-3 pointer going away forces `state` to -1, which retires
/// the task on the following test; a live one moves `killCountdown` a step of
/// 0x100 towards 0x1000 (or zero), then passes it with the slot-3 and slot-0xA
/// objects to `func_800B0928`.
void func_dryfield_night_gas_station_80180828(Task* task)
{
    Task* owner;
    u16   tick;

    owner = gameGetPtrSlot(3);
    if (D_801156F9 == 0) {
        if (owner == 0) {
            task->state = -1;
        }
        if (task->state == 0) {
            if (task->spawnArg1 != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            func_800B0928(owner, gameGetPtrSlot(0xA), 0x300, 0x10, task->killCountdown);
            return;
        }
        taskKill(task);
        D_dryfield_night_gas_station_801907A4 = 0;
    }
}

void func_dryfield_night_gas_station_80180920(s32 arg0)
{
    Gp_ArmStateF0(arg0);
}

/// Spawns entry 1 of the room's task table and tracks it in
/// `D_dryfield_night_gas_station_801907A8`.
void func_dryfield_night_gas_station_80180940(void)
{
    D_dryfield_night_gas_station_801907A8 = Task_SpawnFromTable(&D_dryfield_night_gas_station_801888A0, 1, 0, 0);
}

/// Retires the room's third tracked task and drops the room's reference to it.
/// The `-1` state is the task's own exit request, so the task frees itself on
/// its next tick.
void func_dryfield_night_gas_station_80180974(void)
{
    if (D_dryfield_night_gas_station_801907A8 != NULL) {
        D_dryfield_night_gas_station_801907A8->state = -1;
        D_dryfield_night_gas_station_801907A8        = NULL;
    }
}

/// Runs the room's countdown timer task: for its first 100 ticks it pulses
/// `func_dryfield_night_gas_station_8017FD80` and counts up, then kills itself.
void func_dryfield_night_gas_station_80180998(Task* arg0)
{
    s16 temp_a0;

    if (arg0->state == 0) {
        temp_a0 = arg0->killCountdown;
        if (temp_a0 < 0x64) {
            func_dryfield_night_gas_station_8017FD80(temp_a0);
            arg0->killCountdown = (u16)arg0->killCountdown + 1;
            return;
        }
    }
    taskKill(arg0);
}

/// Spawns the room's second tracked task (entry 2 of the room's task table) and
/// stores it beside `D_dryfield_night_gas_station_801907A8`.
void func_dryfield_night_gas_station_80180A00(void)
{
    D_dryfield_night_gas_station_801907AC = Task_SpawnFromTable(&D_dryfield_night_gas_station_801888A0, 2, 0, 0);
}

/// Advances the room's second tracked task by one state and drops the room's
/// reference to it: the task carries on with its own schedule, untracked.
void func_dryfield_night_gas_station_80180A34(void)
{
    if (D_dryfield_night_gas_station_801907AC != NULL) {
        D_dryfield_night_gas_station_801907AC->state++;
        D_dryfield_night_gas_station_801907AC = NULL;
    }
}

/// Steps the room's blinking-light table: each tick it re-derives whether the
/// current entry's `vx` is odd, and when that flag flips it republishes it to
/// `func_dryfield_night_gas_station_80180DC8` (which switches the lamp effect
/// between its on and off appearance, or back to dark for the -1 state). The
/// index runs to 100 and then wraps.
void func_dryfield_night_gas_station_80180A60(Task* arg0)
{
    s16 temp_v0_2;
    s32 temp_v0;

    if (arg0->state == 0) {
        temp_v0 = (D_dryfield_night_gas_station_80188580[arg0->killCountdown].vx & 1) ^ 1;
        if (arg0->spawnArg1 != temp_v0) {
            arg0->spawnArg1 = temp_v0;
            func_dryfield_night_gas_station_80180DC8((s16)arg0->spawnArg1);
        }
        temp_v0_2           = (u16)arg0->killCountdown + 1;
        arg0->killCountdown = temp_v0_2;
        if (temp_v0_2 >= 0x64) {
            arg0->killCountdown = 0;
        }
    } else {
        func_dryfield_night_gas_station_80180DC8(0);
        taskKill(arg0);
    }
}

/// Spawns the room's third tracked task (entry 3 of the room's task table) and stores
/// it in `D_dryfield_night_gas_station_801907A8`, the slot the room's teardown clears.
void func_dryfield_night_gas_station_80180B04(void)
{
    D_dryfield_night_gas_station_801907A8 = Task_SpawnFromTable(&D_dryfield_night_gas_station_801888A0, 3, 0, 0);
}

/// Second teardown entry point for `D_dryfield_night_gas_station_801907A8`: requests the
/// task's exit and drops the room's reference to it, exactly as
/// `func_dryfield_night_gas_station_80180974` does.
void func_dryfield_night_gas_station_80180B38(void)
{
    if (D_dryfield_night_gas_station_801907A8 != NULL) {
        D_dryfield_night_gas_station_801907A8->state = -1;
        D_dryfield_night_gas_station_801907A8        = NULL;
    }
}

/// Runs the room's countdown task: seeds the RNG on its first tick, then for
/// 100 ticks pulses `func_dryfield_night_gas_station_801802EC` and counts up,
/// then kills itself.
void func_dryfield_night_gas_station_80180B5C(Task* arg0)
{
    s16 temp_a0;

    switch (arg0->state) {
        case 0:
            srand(1);
            arg0->state += 1;
            /* fallthrough */
        case 1:
            temp_a0 = arg0->killCountdown;
            if (temp_a0 < 0x64) {
                func_dryfield_night_gas_station_801802EC(temp_a0);
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
                return;
            }
        default:
            taskKill(arg0);
            return;
    }
}

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
void func_dryfield_night_gas_station_80180BEC(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

void func_dryfield_night_gas_station_80180C20(void)
{
    D_dryfield_night_gas_station_801907A4 = 0;
    D_dryfield_night_gas_station_801907A8 = 0;
    D_dryfield_night_gas_station_801907AC = 0;
}

/// Hides or shows sprite commands 6 and 7 of five of the area's views on its
/// own argument, through their `GpSprtCmd::field_4`: cleared for a 0 argument,
/// set for a 1, and any other argument changes nothing. The first view takes
/// only command 6 and the last two only command 7;
/// `func_dryfield_night_gas_station_80180DC8` drives commands 8 to 10 of the
/// last three views instead.
void func_dryfield_night_gas_station_80180C3C(s32 arg0)
{
    GpAreaKey* sess;
    GpSprtRec* rec;
    GpSprtCmd* view;
    s32        flag;

    sess = &gGameSession->at4.loc;
    rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    flag = arg0 & 0xFF;

    switch (flag) {
        case 0:
            view            = rec[4].field_4;
            view[6].field_4 = 0;
            view            = rec[12].field_4;
            view[6].field_4 = 0;
            view[7].field_4 = 0;
            view            = rec[13].field_4;
            view[6].field_4 = 0;
            view[7].field_4 = 0;
            view            = rec[14].field_4;
            view[7].field_4 = 0;
            view            = rec[16].field_4;
            view[7].field_4 = 0;
            break;
        case 1:
            view            = rec[4].field_4;
            view[6].field_4 = flag;
            view            = rec[12].field_4;
            view[6].field_4 = flag;
            view[7].field_4 = flag;
            view            = rec[13].field_4;
            view[6].field_4 = flag;
            view[7].field_4 = flag;
            view            = rec[14].field_4;
            view[7].field_4 = flag;
            view            = rec[16].field_4;
            view[7].field_4 = flag;
            break;
    }
}

/// Gates the room's two sprite command records on game flag nibble 0x8D: a
/// zero nibble clears both commands' skip-link flag, a one sets it. The two
/// records are views 10 and 19 of the current room's sprite record array, and
/// the flag both write is command 6's.
void func_dryfield_night_gas_station_80180D1C(void)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* view = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1];
    s32        flag = GameFlag_GetNibble(0x8D);

    switch (flag) {
        case 0:
            view[10].field_4[6].field_4 = 0;
            view[19].field_4[6].field_4 = 0;
            break;
        case 1:
            view[10].field_4[6].field_4 = flag;
            view[19].field_4[6].field_4 = flag;
            break;
    }
}

/// Switches the room's lamp effect between its lit and dark appearance: the
/// current room's three lamp views have their three flags written to 1 for the
/// 0 argument and to 0 for the 1 argument, and any other argument changes
/// nothing. `func_dryfield_night_gas_station_80180A60` drives it from the
/// blinking-light table, whose own exit passes 0.
void func_dryfield_night_gas_station_80180DC8(s16 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* rec =
        Gp_SprtTables[sess->stage - 1][0]
            .field_0[sess->area - 1];
    GpSprtCmd* view;

    switch (arg0) {
        case 0:
            view             = rec[13].field_4;
            view[8].field_4  = 1;
            view[9].field_4  = 1;
            view             = rec[14].field_4;
            view[8].field_4  = 1;
            view[9].field_4  = 1;
            view[10].field_4 = 1;
            view             = rec[16].field_4;
            view[8].field_4  = 1;
            view[9].field_4  = 1;
            view[10].field_4 = 1;
            break;
        case 1:
            view             = rec[13].field_4;
            view[8].field_4  = 0;
            view[9].field_4  = 0;
            view             = rec[14].field_4;
            view[8].field_4  = 0;
            view[9].field_4  = 0;
            view[10].field_4 = 0;
            view             = rec[16].field_4;
            view[8].field_4  = 0;
            view[9].field_4  = 0;
            view[10].field_4 = 0;
            break;
    }
}

/// Room effect task tick. The first tick installs the room's three effect ids
/// and sets `roomEffectMode` to 2. Every tick it draws the anchors whose view
/// mask includes the current view. While game flag nibble 0x63 is clear, and
/// outside battles and events, each of the two scatter anchors rolls a jittered
/// spawn position and one of three effects (0x60080, 0x6008D, or 0x60070 on a
/// further 1-in-3). Once the nibble is set, and only if it was seen clear
/// before, the anchors keep spawning 0x60070 alone on a 1-in-3.
void func_dryfield_night_gas_station_80180E9C(Task* task)
{
    DryfieldNightGasStationEffWork* work;
    GpCoord*                        coord;
    s32                             mask;
    s32                             i;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    mask  = 1 << Gp_GetViewIndex();
    if (task->state == 0) {
        D_80115758                 = 0x60006;
        D_8011572C                 = 0x60008;
        D_80115750                 = 0x60009;
        Gp_State1C->roomEffectMode = 2;
    }
    for (i = 0; i < 10; i += 2) {
        if (mask & D_dryfield_night_gas_station_80189D54[i]) {
            func_dryfield_night_gas_station_801812B4(&D_dryfield_night_gas_station_80189C8C[i], 0x180, 0x222);
        }
    }
    if (mask & 0x600C) {
        func_dryfield_night_gas_station_801812B4(&D_dryfield_night_gas_station_80189D34[0], 0x180, 0x444);
        func_dryfield_night_gas_station_801812B4(&D_dryfield_night_gas_station_80189D34[2], 0x180, 0x444);
    }
    for (i = 10; i < 19; i++) {
        if (mask & D_dryfield_night_gas_station_80189D54[i]) {
            func_dryfield_night_gas_station_80181AF8(&D_dryfield_night_gas_station_80189C8C[i], 0, 0x380);
        }
    }
    if (GameFlag_GetNibble(0x63) == 0) {
        work->active = 1;
        if (Gp_State1C->battleState != 1 && Gp_State1C->eventState == 0) {
            for (i = 19; i < 21; i++) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->kind   = (Gp_LcgState >> 16) % 3;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vx = D_dryfield_night_gas_station_80189C8C[i].vx - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                work->pos.vy = D_dryfield_night_gas_station_80189C8C[i].vy;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vz = D_dryfield_night_gas_station_80189C8C[i].vz - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                if (work->kind == 0) {
                    Gp_SpawnEff(0x60080, coord, 0x10300, &work->pos);
                } else if (work->kind == 1) {
                    Gp_SpawnEff(0x6008D, coord, 0x300, &work->pos);
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((u16)((Gp_LcgState >> 16) % 3) == 0) {
                        Gp_SpawnEff(0x60070, coord, 0xC0013500, &work->pos);
                    }
                }
            }
        }
    } else if (work->active != 0 && Gp_State1C->eventState == 0) {
        for (i = 19; i < 21; i++) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((u16)((Gp_LcgState >> 16) % 3) == 0) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vx = D_dryfield_night_gas_station_80189C8C[i].vx - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vy = D_dryfield_night_gas_station_80189C8C[i].vy;
                work->pos.vz = D_dryfield_night_gas_station_80189C8C[i].vz - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                Gp_SpawnEff(0x60070, coord, 0xC0013500, &work->pos);
            }
        }
    }
}

/// Projects the world-space points `arg0[0]` and `arg0[1]` through
/// `Gfx_ViewWorldMtx` and, when both project, joins them with a capsule of
/// gouraud `POLY_G4`s: a half-disc wedge fan around each point and a quad
/// strip between them, three quads per 0x400 step of the angle between the
/// two centres. `arg1` is a signed half-extent scaled by depth; the lit
/// vertices take `arg2` as three 4-bit channels, blended with the frame
/// counter's low bit, and the rim is black.
void func_dryfield_night_gas_station_801812B4(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      sum;
    u8                       r;
    u8                       g;
    u8                       b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// it projects, queues one semi-transparent `POLY_FT4` centred on it: tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380` and the 40-texel texture column
/// `arg1`. `arg2` is a signed half-extent scaled by depth; the grey level
/// follows the frame counter's low bit.
void func_dryfield_night_gas_station_80181AF8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy       = ((RoomDraw13Scratch*)tmp)->sx - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sx + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Effect task of a growing glow: over `spawnArg1` ticks it widens and
/// brightens a disc and ring around the task's object, then flashes the
/// screen and fades a two-ring glow out before releasing itself.
void func_dryfield_night_gas_station_80181D80(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         rgb[3];

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_dryfield_night_gas_station_80182450(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_dryfield_night_gas_station_80182450(coord, (s16)((u16)work->angle * 2), rgb);
                func_dryfield_night_gas_station_80182024(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_dryfield_night_gas_station_80183354(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when it
/// projects, queues sixteen gouraud `POLY_G4` wedges forming a ring: black at
/// the half-extent `arg1` and tinted by `rgb` at `arg1 + arg2`, both scaled
/// by depth.
void func_dryfield_night_gas_station_80182024(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = *(u16*)&arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when it
/// projects, queues eight gouraud `POLY_G4` wedges around it, lit by `rgb` at
/// the centre and black at the rim; `arg1` is a signed half-extent scaled by
/// depth.
void func_dryfield_night_gas_station_80182450(GpCoord* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Effect task of a beam trail: allocates two eight-slot coordinate rings,
/// then each tick records the beam's two end points into the next slot and
/// draws the trail, releasing itself after `spawnArg1` ticks.
void func_dryfield_night_gas_station_801827E4(Task* task)
{
    GpCoord    coord;
    GpCoord*   coords;
    GpCoord*   objCoord;
    GpCoord*   dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = (GpCoord*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GpCoord*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_dryfield_night_gas_station_80189DA0[0].vx;
                objCoord->coord.t[1] = D_dryfield_night_gas_station_80189DA0[0].vy;
                objCoord->coord.t[2] = D_dryfield_night_gas_station_80189DA0[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_dryfield_night_gas_station_80189DA0[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_dryfield_night_gas_station_80189DA8.vx;
                coord.coord.t[1] = D_dryfield_night_gas_station_80189DA8.vy;
                coord.coord.t[2] = D_dryfield_night_gas_station_80189DA8.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_dryfield_night_gas_station_80182CD4(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the trail between the coordinate rings `arg0` and `arg1` as seven
/// gouraud `POLY_G4` quads, walking back from slot `arg2` and fading with
/// age; `arg3` packs three 2-bit colour channels at bits 8, 4 and 0.
void func_dryfield_night_gas_station_80182CD4(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GpCoord*           a;
    GpCoord*           b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    {
        register u8* tmp asm("v0");

        tmp                = SCRATCH_HEAD(u8) - sizeof(RoomDraw03Scratch);
        blk                = (RoomDraw03Scratch*)tmp;
        SCRATCH_HEAD(void) = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// Effect task of a burst: spawns its particle effects, then either scatters
/// sparks for a few ticks (`spawnArg1` non-zero) or expands two rings, and
/// releases itself.
void func_dryfield_night_gas_station_801830CC(Task* task)
{
    GpCoord*   objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.tmd->coords;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_dryfield_night_gas_station_80182024(objCoord, 0x100, 0x100, rgb);
            func_dryfield_night_gas_station_80182024(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when it
/// projects, queues two rings of gouraud `POLY_G4` wedges: an inner one
/// lit by `arg2` at full strength and an outer one at half, both black at the
/// rim. `arg1` is a signed half-extent scaled by depth.
void func_dryfield_night_gas_station_80183354(GpCoord* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}
