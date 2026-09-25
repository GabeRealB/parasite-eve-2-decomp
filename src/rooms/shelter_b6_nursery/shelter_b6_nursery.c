#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
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
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Settings of the room's effect task `func_shelter_b6_nursery_801800A0`,
/// written together by `func_shelter_b6_nursery_80182D14`. A non-zero
/// `field_0` widens the glints of views 3, 6, 8 and 10; a non-zero `field_2`
/// fires a burst of sixteen effects in view 13, scaled by it, after which both
/// are cleared.
typedef struct ShelterB6NurseryPair {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} ShelterB6NurseryPair;
STATIC_ASSERT_SIZEOF(ShelterB6NurseryPair, 0x4);

/// Scratch block one triangle is built in: its three corners in world space,
/// then the GTE depth and flag of its projection.
typedef struct _ShelterB6NurseryTriScratch {
    SVECTOR v[3];
    s32     otz;
    s32     flag;
} _ShelterB6NurseryTriScratch;

s32 rsin(s32);
s32 rcos(s32);

extern void func_80131E2C(void);
extern void func_80132000(void);
extern void func_80132028(void);
extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

extern UiObjectDesc D_800611E4;
extern UiObject*    D_80067634;
extern UiObjectDesc D_8010EFA0;
extern s16          D_80114D08;
extern s32          D_8011572C;
extern s32          D_80115750;
extern s32          D_80115758;
extern s32          D_80139964;
extern s32          D_8013A33C;
extern s32          D_8013A84C;
extern s32          D_8013A8DC;
extern s32          D_8013AF8C;
extern s32          D_8013BA84;

/// `Mc_SaveData.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `Mc_SaveData` address with case 0.

/// View saved when the cutscene starts and restored when it ends.
extern s32 D_80115694;

extern GpAreaApplyRec D_80188888[];

/// Row labels of the play-data statistics list, one per row.
extern u8 D_shelter_b6_nursery_80184CE4[];
extern u8 D_shelter_b6_nursery_80184D14[];
extern u8 D_shelter_b6_nursery_80184CEC[];
extern u8 D_shelter_b6_nursery_80184CF0[];
extern u8 D_shelter_b6_nursery_80184CF8[];
extern u8 D_shelter_b6_nursery_80184D04[];
extern u8 D_shelter_b6_nursery_80184D1C[];
extern u8 D_shelter_b6_nursery_80184D24[];
extern u8 D_shelter_b6_nursery_80184D2C[];

/// Suffix appended to the plain counts.
extern u8 D_shelter_b6_nursery_80184D34[];

/// Suffix appended to the percentages.
extern u8 D_shelter_b6_nursery_80184D3C[];

/// Help lines shown while the matching row is selected.
extern u8 D_shelter_b6_nursery_80184D40[];
extern u8 D_shelter_b6_nursery_80184D6C[];
extern u8 D_shelter_b6_nursery_80184D90[];
extern u8 D_shelter_b6_nursery_80184DC0[];
extern u8 D_shelter_b6_nursery_80184DF4[];
extern u8 D_shelter_b6_nursery_80184E28[];
extern u8 D_shelter_b6_nursery_80184E60[];
extern u8 D_shelter_b6_nursery_80184E94[];
extern u8 D_shelter_b6_nursery_80184ECC[];

/// Lines of the four confirm prompts, and the panels two of them open.
extern u8           D_shelter_b6_nursery_80184CBC[];
extern u8           D_shelter_b6_nursery_80184CC4[];
extern UiObjectDesc D_shelter_b6_nursery_80184F70;
extern u8           D_shelter_b6_nursery_80184CD0[];
extern u8           D_shelter_b6_nursery_80184CDC[];
extern UiObjectDesc D_shelter_b6_nursery_80184F8C;

/// The play-data menu's list, the usage list, the child-panel descriptor both
/// spawn, and the telephone menu's list.
extern UiList       D_shelter_b6_nursery_80184F08;
extern UiList       D_shelter_b6_nursery_80184F30;
extern UiObjectDesc D_shelter_b6_nursery_80184F54;
extern UiList       D_shelter_b6_nursery_80184FB8;

/// Title of the telephone menu. The bytes after its terminator are not zero,
/// so it stays assembly.
extern const char D_shelter_b6_nursery_8017D638[];

/// Task tables: the cutscene and its sound task; the ambient sound task.
extern TaskDesc D_shelter_b6_nursery_80184FDC;
extern TaskDesc D_shelter_b6_nursery_80185000;

/// The room's message table.
extern s32 D_shelter_b6_nursery_8018500C;

/// Per-view depth override for the ambient sound (-1 keeps the computed one).
extern s8 D_shelter_b6_nursery_80185034[];

/// Anchor points of the room's glints and effects.
extern SVECTOR D_shelter_b6_nursery_8018504C[];
extern SVECTOR D_shelter_b6_nursery_80185054;
extern SVECTOR D_shelter_b6_nursery_8018505C[];
extern SVECTOR D_shelter_b6_nursery_80185074;

/// The two ends of the trail effect, relative to its parent coordinate.
extern SVECTOR D_shelter_b6_nursery_801852F4[];
extern SVECTOR D_shelter_b6_nursery_801852FC;

/// The cutscene's running sound task.
extern Task* D_shelter_b6_nursery_80187978;

/// Non-zero while the ambient sound task runs.
extern s32 D_shelter_b6_nursery_8018797C;

extern RoomCutsceneRec D_shelter_b6_nursery_80187980;

/// Position the ambient sound is panned and attenuated from.
extern GpCoord D_shelter_b6_nursery_801879A0;

extern ShelterB6NurseryPair D_shelter_b6_nursery_801879F0;

void func_shelter_b6_nursery_8017F4AC(Task* task);
void func_shelter_b6_nursery_8017FEC4(Task* task);
void func_shelter_b6_nursery_8017FF8C(Task* task);
void func_shelter_b6_nursery_80180518(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b6_nursery_8018098C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b6_nursery_80181EDC(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b6_nursery_80182330(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b6_nursery_801829E4(GpCoord* coord, s16 scale, s16 shade);
void func_shelter_b6_nursery_80182D14(s16 arg0, s16 arg1);
void func_shelter_b6_nursery_80182FCC(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b6_nursery_801833F8(GpCoord* arg0, s32 arg1, u8* rgb);
void func_shelter_b6_nursery_80183C7C(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
void func_shelter_b6_nursery_801842FC(GpCoord* arg0, s16 arg1, u8* arg2);

/// Draws row `field_8` of the play-data statistics list: its label, then its
/// value - a time, a count with its suffix, or a percentage with two decimals
/// built from save counters. Row 5 also draws a separator bar under itself and
/// pushes the following rows down. While the list has focus and the row is the
/// selected one, its help line is posted to the holder.
void func_shelter_b6_nursery_8017D72C(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_shelter_b6_nursery_80184D40,
                D_shelter_b6_nursery_80184D6C,
                D_shelter_b6_nursery_80184D90,
                D_shelter_b6_nursery_80184DC0,
                D_shelter_b6_nursery_80184DF4,
                D_shelter_b6_nursery_80184E28,
                D_shelter_b6_nursery_80184E60,
                D_shelter_b6_nursery_80184E94,
                D_shelter_b6_nursery_80184ECC,
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
            func_8002E53C(&req, D_shelter_b6_nursery_80184CE4);
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
            func_8002E53C(&req, D_shelter_b6_nursery_80184D14);
            Text_ItoaUnsigned(p, Mc_SaveData.saveCount);
            Text_Strcat(p, D_shelter_b6_nursery_80184D34);
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
            func_8002E53C(&req, D_shelter_b6_nursery_80184CEC);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CC);
            Text_Strcat(p, D_shelter_b6_nursery_80184D34);
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
            func_8002E53C(&req, D_shelter_b6_nursery_80184CF0);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CE);
            Text_Strcat(p, D_shelter_b6_nursery_80184D34);
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
            func_8002E53C(&req, D_shelter_b6_nursery_80184CF8);
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
            Text_Strcat(p, D_shelter_b6_nursery_80184D3C);
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
            func_8002E53C(&req, D_shelter_b6_nursery_80184D04);
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
            Text_Strcat(p, D_shelter_b6_nursery_80184D3C);
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
            func_8002E53C(&req, D_shelter_b6_nursery_80184D1C);
            Text_ItoaUnsigned(p, Mc_SaveData.clearCount);
            Text_Strcat(p, D_shelter_b6_nursery_80184D34);
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
            func_8002E53C(&req, D_shelter_b6_nursery_80184D24);
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
            func_8002E53C(&req, D_shelter_b6_nursery_80184D2C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data menu.
const char D_shelter_b6_nursery_8017D610[] = "Play Data";

/// Drawn in place of a percentage for a row holding every recorded use.
const u8 D_shelter_b6_nursery_8017D61C[] = "100.0%";

/// Draws row `field_8` of an item-usage list: the item's name and icon (unless
/// the panel is in mode 5), its share of all uses as a percentage with two
/// decimals (or the fixed text at 100%), and a gouraud bar whose length is the
/// row's 12-bit bar width. While the row is selected it previews the item and
/// posts its text, and a press of button mask 0x10 plays sound 3 and spawns
/// the `D_8010EFA0` panel for the item.
void func_shelter_b6_nursery_8017DEF8(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_shelter_b6_nursery_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_shelter_b6_nursery_80184D3C);
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

/// Fills the weapon-usage list's `RoomItemUsage` block from the save's use
/// counters for items 0x80-0x9F. Every item with a name and a non-zero counter
/// is marked seen and appended, the rows are insertion-sorted most-used first,
/// and each row gets its share of all uses in hundredths of a percent and a
/// bar width as a 12-bit fraction of the top row's counter. The counters are
/// halved as needed until the top one is at most 99999, so neither product can
/// overflow.
void func_shelter_b6_nursery_8017E2F4(UiList* list, UiObject* obj)
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

/// Parasite Energy counterpart of `func_shelter_b6_nursery_8017E2F4`, filling a
/// `RoomPeUsage` block. Each of the twelve PE slots owns three consecutive ids
/// from 0xF, one per level; a slot with a non-zero use counter is listed under
/// the id of its current level. Sorting, percentages and bar widths work as in
/// the weapon list.
void func_shelter_b6_nursery_8017E5F0(UiList* list, UiObject* obj)
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

/// Titles of the usage list: weapons, then Parasite Energy.
const char D_shelter_b6_nursery_8017D624[] = "Weapon Data";
const char D_shelter_b6_nursery_8017D630[] = "PE Data";

/// Usage-list panel task: `spawnArg1` 0 lists weapons, otherwise Parasite
/// Energy. On its first tick it allocates the list's work block, spawns the
/// child panel and fills the list; every tick it updates the list, marks the
/// panel for closing on cancel, and tears down any child panel that has
/// finished.
void func_shelter_b6_nursery_8017E910(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_shelter_b6_nursery_80184F30;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_shelter_b6_nursery_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_shelter_b6_nursery_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F54, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_shelter_b6_nursery_8017E2F4(list, obj);
        } else {
            func_shelter_b6_nursery_8017E5F0(list, obj);
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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", D_shelter_b6_nursery_8017D638);

/// Telephone menu task. Until the save has reached demo scene 1 or been
/// cleared once, it only opens the `D_800611E4` panel; after that it lays out
/// and runs the menu list. A finished child selection opens an item prompt
/// (mode 0x11 for entry 0x33, else 0xF) on the first pass; cancel closes the
/// menu with sound 0x3B.
void func_shelter_b6_nursery_8017EAC4(Task* task)
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
    list          = &D_shelter_b6_nursery_80184FB8;
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
        Ui_DrawText((UiPanel*)obj, D_shelter_b6_nursery_8017D638);
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

/// Prompt-lines panel task: on its first tick it takes over `Wip_UiHolder` and
/// installs the holder-release exit callback, then draws the prompt lines
/// every tick.
void func_shelter_b6_nursery_8017EDBC(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_shelter_b6_nursery_8017F4AC;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into the digit string `str` so that `decimals` digits (or all
/// of them, if fewer) follow it. Does nothing when `decimals` is not positive.
void func_shelter_b6_nursery_8017EE18(u8* str, s32 decimals)
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

/// Formats the fixed-point `value`, which carries `decimals` fractional
/// digits, into `buf` with a '.' before those digits and the percentage suffix
/// after them. Small values are zero-padded so a digit precedes the point.
/// Returns `buf`.
u8* func_shelter_b6_nursery_8017EE88(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_shelter_b6_nursery_80184D3C);
    return buf;
}

/// Play-data menu task: draws its title, lays out the list and spawns the
/// child panel on its first tick, then updates the list every tick and marks
/// the panel for closing on cancel.
void func_shelter_b6_nursery_8017EF7C(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b6_nursery_80184F08;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_shelter_b6_nursery_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F54, 0, 0, 1, obj);
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

/// Queues a gouraud rectangle one ordering-table slot in front of the panel,
/// at offset (`arg1`, `arg2`) from the panel origin, `arg3` wide and `arg4`
/// high. The left edge takes colour `arg5` and the right edge `arg6`. Nothing
/// is drawn when `arg5` is zero or the width is below 2.
void func_shelter_b6_nursery_8017F06C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

/// Confirm-prompt row: draws its line, and on confirm - once the CD is idle -
/// plays sound 0x16, opens the `D_800611E4` panel and hands state 1 to the
/// owner.
void func_shelter_b6_nursery_8017F170(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CBC, prompt->field_1C, 1, 0);
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

/// Confirm-prompt row: draws its line, and on confirm plays sound 0x16, opens
/// the `D_shelter_b6_nursery_80184F70` panel and puts the owner in state 2.
void func_shelter_b6_nursery_8017F254(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CC4, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F70, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Confirm-prompt row: as `func_shelter_b6_nursery_8017F254`, opening the
/// `D_shelter_b6_nursery_80184F8C` panel with argument 0.
void func_shelter_b6_nursery_8017F31C(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CD0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F8C, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Confirm-prompt row: as `func_shelter_b6_nursery_8017F254`, opening the
/// `D_shelter_b6_nursery_80184F8C` panel with argument 1.
void func_shelter_b6_nursery_8017F3E4(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CDC, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F8C, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Exit callback of the prompt-lines panel task: releases `Wip_UiHolder` if the
/// task's panel still holds it, then frees the panel and kills the task.
void func_shelter_b6_nursery_8017F4AC(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

void func_shelter_b6_nursery_8017F4E8(Task* task)
{
    s32              poll;
    s32              cmd;
    s32              a0;
    s32              a1;
    s32              flag;
    s32              key;
    s32              one;
    RoomCutsceneRec* script;
    McSaveData*      save;

    script = task->spawnArg2;
    switch (task->state) {
        case 0:
            D_shelter_b6_nursery_80187978 = NULL;
            Gp_MsgPlayerWeapon(0);
            save = &Mc_SaveData;
            if (save->companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (script->field_0 > 0) {
                D_80115694         = save->at4.loc.view;
                save->at4.loc.view = (u8)script->field_0;
            } else {
                D_80115694 = -script->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (script->field_4 != 0) {
                SndEvt_EnqueueType6(script->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (script->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(script->field_3);
                a0 = script->field_14;
                a1 = 0;
                if (a0 == 0) {
                    a0 = 0x3C0;
                } else {
                    a1 = script->field_16;
                }
                func_800E6D4C(a0, a1);
            }
            if (script->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_shelter_b6_nursery_80187978 =
                Task_SpawnFromTable(&D_shelter_b6_nursery_80184FDC, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_shelter_b6_nursery_80187978);
                task->state++;
            } else if (Task_PollKill(D_shelter_b6_nursery_80187978, &poll) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (script->field_2 == 0) {
                SndEvt_EnqueueType6(script->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            cmd = script->field_1;
            one = 1;
            SOFT_TOUCH_REG(one);
            if (cmd != one) {
                goto L_run_cmd;
            }
            cmd = GameFlag_GetNibble(0x155) + 0x10;
        L_run_cmd:
            Gp_RunCapCmd(cmd, 0);
            flag = GameFlag_GetNibble(0x7A);
            if (flag != 1) {
                goto L_case7_done;
            }
            if (GameFlag_GetNibble(0) == 2) {
                GameFlag_SetNibble(0, 3);
                GameFlag_SetNibble(0xE, 4);
                if ((GP_LOC_WORD(Mc_SaveData.at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 1, 0, 0)) {
                    Gp_ApplyAreaRecs(D_80188888);
                    func_800E3FAC(0xA2, 5);
                }
            }
        L_case7_done:
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if (GameFlag_GetNibble(0x155) == 0xE) {
                    if (GameFlag_GetNibble(3) == 0) {
                        GameFlag_SetNibble(3, 1);
                        task->state = 0x14;
                        break;
                    }
                }
                Gp_RunCapCmd1(task->spawnArg1);
                task->state++;
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData.at4.loc.view = (u8)D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(script->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (script->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            key = Gp_GetCapEventKey();
            if (key == 0xB) {
                goto L_keyB;
            }
            if (key == 0xC) {
                goto L_keyC;
            }
            goto L_keyDefault;
        L_keyB:
            Gp_RunCapCmd(0x20, 0);
            task->state++;
            break;
        L_keyC:
            Gp_RunCapCmd(0x21, 0);
            task->state++;
            break;
        L_keyDefault:
            GameFlag_SetNibble(3, 2);
            task->state = 8;
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

/// States of the room's message task, run by
/// `func_shelter_b6_nursery_8017FF9C`: install the message table, idle, die.
const TaskFuncTable3 D_shelter_b6_nursery_8017D6A4 = {
    {
        func_shelter_b6_nursery_8017FEC4,
        func_shelter_b6_nursery_8017FF8C,
        taskKill,
    },
};

s32 func_shelter_b6_nursery_8017FA54(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 flag;

    if (arg2 == 0xA) {
        D_shelter_b6_nursery_8018797C          = 0;
        D_shelter_b6_nursery_80187980.field_4  = 0x55160002;
        D_shelter_b6_nursery_80187980.field_8  = 0x55160005;
        D_shelter_b6_nursery_80187980.field_10 = 0x55160003;
        D_shelter_b6_nursery_80187980.field_C  = 0x55160004;
        flag                                   = GameFlag_GetNibble(0xC7);
        if (flag == 1) {
            if (GameFlag_GetNibble(0x83) != 0) {
                Gp_SetBit2Flag(0x22, 1, 4);
            }
            func_800E3FAC(0xA2, 0x31);
            GameFlag_SetNibble(0xC7, 2);
            D_shelter_b6_nursery_80187980.field_0 = 6;
            D_shelter_b6_nursery_80187980.field_1 = 0xB;
            D_shelter_b6_nursery_80187980.field_3 = 0;
            D_shelter_b6_nursery_80187980.field_2 = flag;
            Task_SpawnFromTable(&D_shelter_b6_nursery_80184FDC, 0, 0x19,
                                (s32)&D_shelter_b6_nursery_80187980);
            func_80132028();
            func_shelter_b6_nursery_80182D14(0, 0);
            return 0;
        }
        if (GameFlag_GetNibble(0x160) == 0) {
            Gp_SpawnIfCapIdle(0x17, 0);
            GameFlag_SetNibble(0x160, 1);
            return 0;
        }
        D_shelter_b6_nursery_80187980.field_0 = 6;
        D_shelter_b6_nursery_80187980.field_1 = 0x16;
        D_shelter_b6_nursery_80187980.field_3 = 0;
        D_shelter_b6_nursery_80187980.field_2 = 0;
        Task_SpawnFromTable(&D_shelter_b6_nursery_80184FDC, 0, 0xA,
                            (s32)&D_shelter_b6_nursery_80187980);
    }
    return 0;
}

void func_shelter_b6_nursery_8017FBC0(Task* arg0)
{
    s32 pan;
    s32 depth;
    s32 viewDepth;

    D_shelter_b6_nursery_801879A0.coord.t[0] = 0x1770;
    D_shelter_b6_nursery_801879A0.coord.t[1] = 0;
    D_shelter_b6_nursery_801879A0.coord.t[2] = -0x33E;
    D_shelter_b6_nursery_801879A0.sub        = &gGfxViewCoord;
    D_shelter_b6_nursery_801879A0.flg        = 0;
    Gp_UpdateCoord(&D_shelter_b6_nursery_801879A0);
    pan   = Gp_GetObjPan(&D_shelter_b6_nursery_801879A0);
    depth = gpGetObjDepth(&D_shelter_b6_nursery_801879A0);
    switch (arg0->state) {
        case 0:
            SndEvt_EnqueueType6(0x55160001, (s8)pan, (s8)depth);
            arg0->state++;
            break;
        case 1:
            if (D_shelter_b6_nursery_8018797C == 0) {
                SndEvt_EnqueueType7(0x55160001, 1);
                taskKill(arg0);
                return;
            }
            if (Mc_SaveData.at4.loc.view != gGameSession->at4.loc.view) {
                arg0->state++;
            }
            break;
        case 2:
        case 3:
        case 4:
            arg0->state++;
            break;
        case 5:
            viewDepth = D_shelter_b6_nursery_80185034[gGameSession->at4.loc.view];
            if (viewDepth != -1) {
                depth = viewDepth;
            }
            SndEvt_EnqueueTypeA(0x55160001, (s8)pan, (s8)depth);
            arg0->state = 1;
            break;
    }
}

/// Sound task: plays the sound event `spawnArg2` on its first tick and again on
/// tick 0x50, and asks to be killed on tick 0x78.
void func_shelter_b6_nursery_8017FD3C(Task* task)
{
    s32 zero;

    switch (task->state) {
        case 0x50:
        case 0x0:
            zero = 0;
            TOUCH_REG(zero);
            SndEvt_EnqueueType6((s32)task->spawnArg2, zero, zero);
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            return;
    }
    task->state += 1;
}

/// Always answers 0.
s32 func_shelter_b6_nursery_8017FDCC(void)
{
    return 0;
}

s32 func_shelter_b6_nursery_8017FDD4(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179B14(src, dst);
    if (src->field_5 == 0) {
        Gp_RunCapCmd1(0xC);
    }
    return 0;
}

s32 func_shelter_b6_nursery_8017FE3C(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    if (msg->field_2 == 1) {
        func_80131E2C();
    }
    if (msg->field_2 == 2) {
        func_80132000();
    }
    if (msg->field_2 == 3 && GameFlag_GetNibble(0xC8) != 0) {
        func_800E8634((s32)&D_8013AF8C, 0, (s32)&D_8013BA84);
    }
    return 0;
}

void func_shelter_b6_nursery_8017FEC4(Task* arg0)
{
    arg0->msgTable = &D_shelter_b6_nursery_8018500C;
    Game_SetPtrSlot(arg0, 7);
    Gp_FillAllyHp();
    if (GameFlag_GetNibble(0xC7) == 0) {
        GameFlag_SetNibble(0xC7, 1);
        func_800E8634((s32)&D_80139964, 0, (s32)&D_8013A33C);
        GameFlag_SetNibble(3, 0);
        func_800E3FAC(0xA2, 0x30);
    } else if (GameFlag_GetNibble(0xC7) == 1) {
        func_800E8614((s32)&D_8013A84C, 1);
    } else {
        func_800E8614((s32)&D_8013A8DC, 1);
    }
    arg0->state++;
}

/// Idle state of the room's message task: does nothing. The unused local
/// reproduces the original's stack frame.
void func_shelter_b6_nursery_8017FF8C(Task* task)
{
    char pad[0x10];
}

/// Runs the room's message task: calls the state handler `task->state` selects
/// from a stack copy of its three-entry table.
void func_shelter_b6_nursery_8017FF9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_nursery_8017D6A4;
    sp.funcs[task->state](task);
}

void func_shelter_b6_nursery_8017FFF4(void)
{
    if (D_shelter_b6_nursery_8018797C == 0) {
        D_shelter_b6_nursery_8018797C = 1;
        Task_SpawnFromTable(&D_shelter_b6_nursery_80185000, 0, 0, 0);
    }
}

/// Sets the second sprite command's skip-link flag in view 13 for the current
/// room in the first stage table. Only low-byte values 0 and 1 change the flag.
void func_shelter_b6_nursery_80180038(s32 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;
    s32        mode;

    cmd  = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][12].field_4;
    mode = arg0 & 0xFF;
    if (mode == 0) {
        cmd[1].field_4 = 0;
    } else if (mode == 1) {
        cmd[1].field_4 = 1;
    }
}

void func_shelter_b6_nursery_801800A0(Task* task)
{
    SVECTOR* pos;
    u32      a;
    u32      b;
    s32      angle;
    s32      r;
    s32      i;

    if (task->state == 0) {
        D_80115758                            = 0x601E0;
        D_8011572C                            = 0x601FC;
        D_80115750                            = 0x60218;
        D_shelter_b6_nursery_801879F0.field_0 = 0;
        D_shelter_b6_nursery_801879F0.field_2 = 0;
        task->state                           = 1;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
        case 8:
            if (D_shelter_b6_nursery_801879F0.field_0 != 0) {
                func_shelter_b6_nursery_80180518(D_shelter_b6_nursery_8018504C, 0x180, 0x80);
            } else {
                func_shelter_b6_nursery_80180518(D_shelter_b6_nursery_8018504C, 0x60, 0x80);
            }
            break;
        case 6:
        case 10:
            if (D_shelter_b6_nursery_801879F0.field_0 != 0) {
                func_shelter_b6_nursery_8018098C(D_shelter_b6_nursery_8018504C, 0x180, 0x80);
            } else {
                func_shelter_b6_nursery_8018098C(D_shelter_b6_nursery_8018504C, 0x60, 0x80);
            }
            break;
        case 12:
            if (task->state == 1) {
                Gp_SpawnEff(0x601A3, NULL, task->spawnArg1, &D_shelter_b6_nursery_80185054);
                Gp_SpawnEff(0x601A3, NULL, task->spawnArg1, &D_shelter_b6_nursery_80185054);
                Gp_SpawnEff(0x601A3, NULL, task->spawnArg1, &D_shelter_b6_nursery_80185054);
                task->state = 2;
            }
            break;
        case 13:
            task->state = 3;
            if (D_shelter_b6_nursery_801879F0.field_2 != 0) {
                for (i = 0; i < 16; i++) {
                    a                                   = Gp_LcgState * 5 + 0x71357911;
                    b                                   = a * 5 + 0x71357911;
                    angle                               = (a >> 16) & 0xFFF;
                    Gp_LcgState                         = b;
                    r                                   = ((b >> 16) & 0xFF) * D_shelter_b6_nursery_801879F0.field_2;
                    D_shelter_b6_nursery_8018504C[6].vx = 0x1C20;
                    D_shelter_b6_nursery_8018504C[6].vy = ((r * rsin(angle)) >> 12) - 0x6D6;
                    D_shelter_b6_nursery_8018504C[6].vz = ((r * rsin(angle)) >> 12) + 0x7D0;
                    Gp_LcgState                         = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601A5, NULL,
                                (((Gp_LcgState >> 16) & 0x1F) + 8) * D_shelter_b6_nursery_801879F0.field_2,
                                &D_shelter_b6_nursery_8018504C[6]);
                }
                D_shelter_b6_nursery_801879F0.field_0 = 0;
                D_shelter_b6_nursery_801879F0.field_2 = 0;
            }
            break;
        case 15:
            if (!(gDisplayState.animFrame & 1)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_80185074);
            }
            break;
        case 17:
            pos = D_shelter_b6_nursery_8018504C;
            func_shelter_b6_nursery_80180518(pos, 0x60, 0x80);
            if (!(gDisplayState.animFrame & 1)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[4]);
            }
            break;
        case 18:
            if (!(gDisplayState.animFrame & 1)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[4]);
            }
            break;
    }
    if (task->state == 3 && !(gDisplayState.animFrame & 1)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018505C[0]);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x601A4, NULL, ((Gp_LcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018505C[1]);
    }
}

/// Draws a glint at the world point `arg0`, projected through
/// `gGfxViewCoord.workm`: two gouraud quads forming a diamond and two gouraud
/// three-point lines across it, of radius `(s16)arg2 * 32` over the depth.
/// The lit vertices pulse in green and blue at a rate of `(s16)arg1` times the
/// animation frame. Nothing is drawn when the projection flags an error.
void func_shelter_b6_nursery_80180518(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x10);
        block   = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, pulse, pulse);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x10);
}

/// Draws a star-shaped glow at the world point `arg0`, projected through
/// `gGfxViewCoord.workm`: a disc of gouraud wedges at radius `(s16)arg2 * 64`
/// over the depth, a brighter disc at half that radius, and four cross
/// wedges reaching out from an inner radius of `(s16)arg2 * 8` over the depth.
/// The lit vertices pulse in green and blue at a rate of `(s16)arg1` times the
/// animation frame. Nothing is drawn when the projection flags an error.
void func_shelter_b6_nursery_8018098C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                pulse;
    s32                color;
    s32                half;
    s32                size;
    s32                ang;
    s32                t;
    s32                t2;
    s32                u;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw05Scratch*)(head - 0x14))->sx);
    gte_stflg(&((RoomDraw05Scratch*)(head - 0x14))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomDraw05Scratch*)(head - 0x14))->otz;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
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
            setRGB2(prim, 0, color, color);
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

        color = half;
        ang   = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
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
            setRGB2(prim, 0, color, color);
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
    SCRATCH_POP_BYTES(0x14);
}

void func_shelter_b6_nursery_80181314(Task* task)
{
    SVECTOR    step;
    SVECTOR    pos;
    SVECTOR    base;
    TmdObject* obj;
    GpEffWork* work;
    GpCoord*   coord;
    s16        eventState;

    obj   = task->extra.tmd;
    work  = task->spawnArg2;
    coord = obj->coords;
    if ((Gp_GetViewIndex() & 0xFF) != 0xC) {
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    {
        eventState = Gp_State1C->eventState;
        if (eventState >= 2) {
            if (eventState >= 4) {
                Gp_ReleaseState1CMem(work, task);
            }
        } else {
            Gp_UpdateCoord(coord);
            if (task->state == 0) {
                obj->flags   &= 0xFF7F;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vx = ((Gp_LcgState >> 16) & 0x3F) + 0x60;
                work->move.vy = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vz = ((Gp_LcgState >> 16) & 0x3F) + 0x20;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->scale   = ((Gp_LcgState >> 16) & 0x3F) + 0x40;
                VectorNormalSS(&work->move, &work->move);
                work->pos.vy = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vx = -((Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vz = 0x40 - ((Gp_LcgState >> 16) & 0x7F);
                coord->flg   = 0;
                task->state++;
                return;
            }
            Gfx_RotMatrixXYZ(&coord->coord, (SVECTOR*)&work->pos.vx, 0);
            MatrixNormal(&coord->coord, &coord->coord);
            gte_lddp(work->scale);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&step);
            coord->coord.t[0] += step.vx;
            coord->coord.t[1] += step.vy;
            coord->coord.t[2] += step.vz;
            coord->flg         = 0;
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&step);
            gte_rtv0();
            gte_stsv(&pos);
            base.vx = coord->workm.t[0];
            base.vy = coord->workm.t[1];
            base.vz = coord->workm.t[2];
            pos.vx += base.vx;
            pos.vy += base.vy;
            pos.vz += base.vz;
            if (func_800DE7CC(&pos, &base, &pos, &base) == 1) {
                coord->coord.t[0] -= step.vx;
                coord->coord.t[1] -= step.vy;
                coord->coord.t[2] -= step.vz;
                work->move.vx      = (base.vx >> 1) + (work->move.vx >> 1);
                work->move.vy      = base.vy + (work->move.vy >> 1);
                work->move.vz      = (base.vz >> 1) + (work->move.vz >> 1);
                VectorNormalSS(&work->move, &work->move);
                work->scale = work->scale * 2 / 3;
                gte_lddp(work->scale);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&step);
                coord->coord.t[0] += step.vx;
                coord->coord.t[1] += step.vy;
                coord->coord.t[2] += step.vz;
            } else {
                work->move.vy += 0x180;
            }
            if (work->age & 1) {
                if (work->age > 0x40) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601A4, coord, ((Gp_LcgState >> 16) & 0x10FF) + 0x02183300, NULL);
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x601A4, coord, ((Gp_LcgState >> 16) & 0x1000) + 0x82101300, NULL);
                }
            }
            work->age++;
        }
    }
}

void func_shelter_b6_nursery_80181820(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;
    s32        step;
    s32        level;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            task->state  = task->spawnArg1 < 0 ? 2 : 1;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1 >> 24) & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case 6:
                        work->move.vy = 0;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 7:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = ((u32)Gp_LcgState >> 16) & 0xFF;
                        gte_SetRotMatrix(&work->parent->coord);
                        gte_ldv0(&work->move);
                        gte_rtv0();
                        gte_stsv(&work->move);
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            break;
        case 1:
            func_shelter_b6_nursery_80181EDC(coord, work->index, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b6_nursery_80182330(coord, work->index, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 8) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

void func_shelter_b6_nursery_80181EDC(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              vz;
    u16              col;
    u16              row;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)coord->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)coord->workm.t[1];
    vz                                        = (u16)coord->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        if (block->otz >= 0x41) {
            prim->tpage = 0x2B;
            prim->clut  = 0x4384;
            prim->code |= 3;
            col         = arg1 % 5;
            row         = arg1 / 5;
            u0          = col * 48;
            v0          = row * 48;
            setUV4(prim, u0, v0 + 0x28, u0 + 0x2F, v0 + 0x28, u0, v0 + 0x57, u0 + 0x2F, v0 + 0x57);
            ang       = arg3;
            block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = block->sx + (u16)block->dx;
            prim->x3  = block->sx - (u16)block->dx;
            prim->y0  = block->sy - (u16)block->dy;
            ang2      = ang + 0x400;
            prim->y3  = block->sy + (u16)block->dy;
            block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
            block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = block->sx + (u16)block->dx;
            prim->x2  = block->sx - (u16)block->dx;
            prim->y1  = block->sy - (u16)block->dy;
            prim->y2  = block->sy + (u16)block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

void func_shelter_b6_nursery_80182330(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)coord->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)coord->workm.t[1];
    vz                                        = (u16)coord->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        if (block->otz >= 0x41) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x2B;
            prim->clut  = 0x4385;
            u0          = (arg1 & 7) << 5;
            u1          = u0 + 0x1F;
            setUV4(prim, u0, 0x88, u1, 0x88, u0, 0xA7, u1, 0xA7);
            ang       = arg3;
            block->dx = (((arg2 * 31) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 31) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = block->sx + (u16)block->dx;
            prim->x3  = block->sx - (u16)block->dx;
            prim->y0  = block->sy - (u16)block->dy;
            ang2      = ang + 0x400;
            prim->y3  = block->sy + (u16)block->dy;
            block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
            block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = block->sx + (u16)block->dx;
            prim->x2  = block->sx - (u16)block->dx;
            prim->y1  = block->sy - (u16)block->dy;
            prim->y2  = block->sy + (u16)block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

void func_shelter_b6_nursery_80182730(Task* task)
{
    SVECTOR    step;
    GpEffWork* work;
    GpCoord*   coord;
    s16        eventState;

    work       = task->spawnArg2;
    eventState = Gp_State1C->eventState;
    coord      = task->extra.tmd->coords;
    if (eventState >= 2) {
        if (eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        if (task->state == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->scale   = ((Gp_LcgState >> 16) & 0x3F) + 0x40;
            work->angle   = task->spawnArg1 & 0xFFF;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = ((Gp_LcgState >> 16) & 0x7F) + 0x40;
            VectorNormalSS(&work->move, &work->move);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->pos.vx = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->pos.vy = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->pos.vz = 0x100 - ((Gp_LcgState >> 16) & 0x1FF);
            coord->flg   = 0;
            task->state++;
            return;
        }
        Gfx_RotMatrixXYZ(&coord->coord, (SVECTOR*)&work->pos.vx, 0);
        MatrixNormal(&coord->coord, &coord->coord);
        gte_lddp(work->scale);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&step);
        coord->coord.t[0] += step.vx;
        coord->coord.t[1] += step.vy;
        coord->coord.t[2] += step.vz;
        coord->flg         = 0;
        func_shelter_b6_nursery_801829E4(coord, work->angle, work->period);
        if (coord->coord.t[1] > 0) {
            Gp_ReleaseState1CMem(work, task);
        } else {
            work->move.vy += 0x180;
        }
    }
}

/// Draws one flat grey triangle at `coord`: three corners at 120-degree steps
/// on a circle of radius `scale` in the coordinate's YZ plane, transformed by
/// its world matrix, projected with `GsWSMATRIX` and linked into the ordering
/// table at the triangle's depth with shade `shade`.
void func_shelter_b6_nursery_801829E4(GpCoord* coord, s16 scale, s16 shade)
{
    _ShelterB6NurseryTriScratch* blk;
    SVECTOR*                     p;
    POLY_F3*                     prim;
    s32                          i;

    SCRATCH_PUSH(_ShelterB6NurseryTriScratch);
    blk = SCRATCH_HEAD(_ShelterB6NurseryTriScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 3; i++) {
        p     = &blk->v[i];
        p->vx = 0;
        p->vy = rsin(i * 0x555);
        p->vz = rcos(i * 0x555);
        gte_lddp(scale);
        gte_ldsv(p);
        gte_gpf12();
        gte_stsv(p);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(p);
        gte_rtv0();
        gte_stsv(p);
        p->vx = (u16)p->vx + (u16)coord->workm.t[0];
        p->vy = (u16)p->vy + (u16)coord->workm.t[1];
        p->vz = (u16)p->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&blk->v[0], &blk->v[1], &blk->v[2]);
    gte_rtpt();
    prim           = (POLY_F3*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyF3(prim);
    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
    gte_stflg(&blk->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&blk->otz);
        setRGB0(prim, shade, shade, shade);
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_AddTpageShift((P_TAG*)prim, (Gp_LcgState >> 16) & 1, blk->otz);
    }
    SCRATCH_POP(_ShelterB6NurseryTriScratch);
}

void func_shelter_b6_nursery_80182D14(s16 arg0, s16 arg1)
{
    D_shelter_b6_nursery_801879F0.field_0 = arg0;
    D_shelter_b6_nursery_801879F0.field_2 = arg1;
}

/// Flash effect task on the object's coordinate. Over `spawnArg1` ticks it
/// brightens, drawing two fans and a ring in a red-dominant colour that grows
/// each tick; on the last one it flashes the screen. It then fades out as a
/// shrinking billboard, 0x10 a tick, and releases its work block. It also
/// releases it once the room's event state reaches 4, and is frozen while the
/// event state is non-zero.
void func_shelter_b6_nursery_80182D28(Task* task)
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
                func_shelter_b6_nursery_801833F8(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b6_nursery_801833F8(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b6_nursery_80182FCC(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_shelter_b6_nursery_801842FC(coord, (s16)(work->angle * 3), rgb);
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

/// Draws a ring of sixteen gouraud quads around the screen position of the
/// coordinate's world translation, unless the projection flags an error. The
/// vertices at radius `(s16)arg1 * 64` over the depth are black and those at
/// `(s16)(arg1 + arg2) * 64` over the depth take `rgb`.
void func_shelter_b6_nursery_80182FCC(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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
    vx      = (u16)arg0->workm.t[0];
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
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
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a fan of eight gouraud quads around the screen position of the
/// coordinate's world translation, unless the projection flags an error: the
/// centre takes `rgb` and the rim, at radius `(s16)arg1 * 64` over the depth,
/// is black.
void func_shelter_b6_nursery_801833F8(GpCoord* arg0, s32 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    register void** scratch asm("a1");
    u8*             head;
    s32             otz;
    s32             radius;
    s32             t;
    s32             t2;
    u16             vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                       = (u16)arg0->workm.t[0];
        ((RoomFanScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomFanScratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomFanScratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomFanScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomFanScratch*)(head - 0x18))->otz);
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
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Trail effect task. On its first tick it allocates two eight-slot histories
/// of view-space coordinates and fills both with the trail's two ends; then
/// every tick it records the current ends in the next slot and draws the band
/// between the histories, until its age reaches `spawnArg1`. It is frozen
/// while the room's event state is 2 or more.
void func_shelter_b6_nursery_8018378C(Task* task)
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
                objCoord->coord.t[0] = D_shelter_b6_nursery_801852F4[0].vx;
                objCoord->coord.t[1] = D_shelter_b6_nursery_801852F4[0].vy;
                objCoord->coord.t[2] = D_shelter_b6_nursery_801852F4[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_b6_nursery_801852F4[1];
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
                coord.coord.t[0] = D_shelter_b6_nursery_801852FC.vx;
                coord.coord.t[1] = D_shelter_b6_nursery_801852FC.vy;
                coord.coord.t[2] = D_shelter_b6_nursery_801852FC.vz;
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
                func_shelter_b6_nursery_80183C7C(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the band between two eight-slot coordinate histories as seven gouraud
/// quads, walking back from slot `arg2`; each quad joins two consecutive slots
/// of `arg0` and `arg1`. Brightness falls by 9 per quad from 0x40, and `arg3`
/// scales it per channel: red by `arg3 >> 8`, green by bits 4-5 and blue by
/// bits 0-1. A quad whose projection flags an error is skipped.
void func_shelter_b6_nursery_80183C7C(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
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
        blk->v[0].vx = (u16)a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = (u16)a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = (u16)a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        blk->v[1].vz = (u16)b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        blk->v[2].vz = (u16)a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
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

/// Burst effect task on the object's coordinate. On its first tick it spawns
/// effect 0x60076, then either (`spawnArg1` non-zero) a spray of 0x60070
/// sparks in random directions for seven ticks, or two 0x6007C effects and
/// seven ticks of a fading, widening ring. It then releases its work block,
/// as it does once the room's event state reaches 4; it is frozen while the
/// event state is non-zero.
void func_shelter_b6_nursery_80184074(Task* task)
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
            func_shelter_b6_nursery_80182FCC(objCoord, 0x100, 0x100, rgb);
            func_shelter_b6_nursery_80182FCC(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a star-shaped glow around the screen position of the coordinate's
/// world translation, unless the projection flags an error: a fan of gouraud
/// wedges at radius `arg1 * 64` over the depth in half of `arg2`'s colour, a
/// second at half that radius in the full colour, and four cross wedges from
/// an inner radius of `arg1 * 8` over the depth. Every rim is black.
void func_shelter_b6_nursery_801842FC(GpCoord* arg0, s16 arg1, u8* arg2)
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
        vx                                             = (u16)arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
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
