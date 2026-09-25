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
#include "main/ui.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// 0x18-byte block `func_shelter_b2_laboratory_801812F8` takes from
/// `G_SCRATCH_HEAD`: the projected centre `sx` / `sy`, its `otz` and GTE
/// `flag`, and the on-screen `radius`. Nothing here reads the bytes between
/// `radius` and `sx`, so what the block keeps there is unknown.
typedef struct {
    s32 otz;
    s32 flag;
    s32 radius;
    u8  _pad[8];
    u16 sx;
    u16 sy;
} _DrawScratch;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern UiObjectDesc   D_800611E4;
extern UiObject*      D_80067634;
extern u16            D_8007A39C;
extern UiObjectDesc   D_8010EFA0;
extern s16            D_80114D08;
extern TaskDesc       D_80134564;
extern GpAreaApplyRec D_80188888[];

/// View saved when the cutscene starts and restored when it ends.
extern s32 D_80115694;

/// Title of the telephone menu. The bytes after its terminator are not zero,
/// so it stays assembly.
extern const char D_shelter_b2_laboratory_8017D638[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
extern u8 D_shelter_b2_laboratory_80182100[];
extern u8 D_shelter_b2_laboratory_80182108[];
extern u8 D_shelter_b2_laboratory_80182114[];
extern u8 D_shelter_b2_laboratory_80182120[];

/// Row captions of the play-data panel, one per row.
extern u8 D_shelter_b2_laboratory_80182128[];
extern u8 D_shelter_b2_laboratory_80182158[];
extern u8 D_shelter_b2_laboratory_80182130[];
extern u8 D_shelter_b2_laboratory_80182134[];
extern u8 D_shelter_b2_laboratory_8018213C[];
extern u8 D_shelter_b2_laboratory_80182148[];
extern u8 D_shelter_b2_laboratory_80182160[];
extern u8 D_shelter_b2_laboratory_80182168[];
extern u8 D_shelter_b2_laboratory_80182170[];

/// Suffix appended after a plain count on rows 1, 2, 3 and 6 of the play-data
/// panel.
extern u8 D_shelter_b2_laboratory_80182178[];

/// Suffix appended after a percentage.
extern u8 D_shelter_b2_laboratory_80182180[];

/// Help strings handed to the UI holder while the cursor rests on a row of
/// the play-data panel, one per row.
extern u8 D_shelter_b2_laboratory_80182184[];
extern u8 D_shelter_b2_laboratory_801821B0[];
extern u8 D_shelter_b2_laboratory_801821D4[];
extern u8 D_shelter_b2_laboratory_80182204[];
extern u8 D_shelter_b2_laboratory_80182238[];
extern u8 D_shelter_b2_laboratory_8018226C[];
extern u8 D_shelter_b2_laboratory_801822A4[];
extern u8 D_shelter_b2_laboratory_801822D8[];
extern u8 D_shelter_b2_laboratory_80182310[];

/// Lists of the play-data panel, the usage panel and the telephone menu, and
/// the descriptors of the panels they spawn.
extern UiList       D_shelter_b2_laboratory_8018234C;
extern UiList       D_shelter_b2_laboratory_80182374;
extern UiObjectDesc D_shelter_b2_laboratory_80182398;
extern UiObjectDesc D_shelter_b2_laboratory_801823B4;
extern UiObjectDesc D_shelter_b2_laboratory_801823D0;
extern UiList       D_shelter_b2_laboratory_801823FC;

/// Task tables the room spawns from: the cutscene, the flag-gated exit's
/// transition, and the console's tasks.
extern TaskDesc D_shelter_b2_laboratory_80182A08[];
extern TaskDesc D_shelter_b2_laboratory_80182A2C;
extern TaskDesc D_shelter_b2_laboratory_80182A6C[];

/// Message table of the room's message task.
extern GpMsgEntry D_shelter_b2_laboratory_80182A38[];

/// Task spawned by `func_shelter_b2_laboratory_80180290` and polled until it
/// dies.
extern Task* D_shelter_b2_laboratory_80182A68;

/// Per-view attenuation, in percent, of the looping sound played by
/// `func_shelter_b2_laboratory_8017FEB8`.
extern s8 D_shelter_b2_laboratory_80182A90[];

/// Glow positions `func_shelter_b2_laboratory_80180548` draws per view:
/// `func_shelter_b2_laboratory_80180AB4` takes the pair `pt[n]`, `pt[n + 1]`;
/// `func_shelter_b2_laboratory_801812F8` and
/// `func_shelter_b2_laboratory_8018176C` a single point.
extern SVECTOR D_shelter_b2_laboratory_80182AA0[];
extern SVECTOR D_shelter_b2_laboratory_80182AB0[];
extern SVECTOR D_shelter_b2_laboratory_80182B00[];
extern SVECTOR D_shelter_b2_laboratory_80182B20[];
extern SVECTOR D_shelter_b2_laboratory_80182B60[];
extern SVECTOR D_shelter_b2_laboratory_80182B70[];
extern SVECTOR D_shelter_b2_laboratory_80182BF0[];
extern SVECTOR D_shelter_b2_laboratory_80182C00[];

/// The cutscene's sound task, killed when the scene is skipped.
extern Task* D_shelter_b2_laboratory_801864A4;

/// Copies of the message and request that started the pending exit
/// transition, read back by `func_shelter_b2_laboratory_8017FBA8`.
extern RoomEventMsg D_shelter_b2_laboratory_801864AC;
extern RoomEventReq D_shelter_b2_laboratory_8018652C;

/// Set when `func_shelter_b2_laboratory_8017FA44` started a transition,
/// cleared on every other call.
extern u8 D_shelter_b2_laboratory_801864B4;

/// Non-zero while the looping sound of `func_shelter_b2_laboratory_8017FEB8`
/// should keep playing; set by `func_shelter_b2_laboratory_801804FC`.
extern s32 D_shelter_b2_laboratory_801864B8;

/// Parameters of the cutscene `func_shelter_b2_laboratory_8017FD18` starts.
extern RoomCutsceneRec D_shelter_b2_laboratory_801864BC;

/// World position the looping sound is panned and attenuated from.
extern GpCoord D_shelter_b2_laboratory_801864DC;

/// Non-zero makes the view glows of `func_shelter_b2_laboratory_80180548`
/// pulse faster. Written through `func_shelter_b2_laboratory_801820F4`; the
/// glow task clears it when it starts.
extern u16 D_shelter_b2_laboratory_80186540;

void func_shelter_b2_laboratory_8017F49C(Task* task);
void func_shelter_b2_laboratory_80180450(Task* task);
void func_shelter_b2_laboratory_80180494(Task* task);
void func_shelter_b2_laboratory_80180AB4(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_laboratory_801812F8(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_laboratory_8018176C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_laboratory_801820F4(s16 arg0);

/// Draws one row of the play-data panel, the row picked by
/// `DialogPrompt::field_8`: a caption followed by a value - play time, one of
/// several counters with a unit suffix, or a percentage kept in hundredths
/// whose decimal point is inserted by hand (row 5 also draws a gauge and takes
/// an extra line). While the cursor is on the row its help string is shown.
void func_shelter_b2_laboratory_8017D71C(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_shelter_b2_laboratory_80182184,
                D_shelter_b2_laboratory_801821B0,
                D_shelter_b2_laboratory_801821D4,
                D_shelter_b2_laboratory_80182204,
                D_shelter_b2_laboratory_80182238,
                D_shelter_b2_laboratory_8018226C,
                D_shelter_b2_laboratory_801822A4,
                D_shelter_b2_laboratory_801822D8,
                D_shelter_b2_laboratory_80182310,
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
            func_8002E53C(&req, D_shelter_b2_laboratory_80182128);
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
            func_8002E53C(&req, D_shelter_b2_laboratory_80182158);
            Text_ItoaUnsigned(p, Mc_SaveData.saveCount);
            Text_Strcat(p, D_shelter_b2_laboratory_80182178);
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
            func_8002E53C(&req, D_shelter_b2_laboratory_80182130);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CC);
            Text_Strcat(p, D_shelter_b2_laboratory_80182178);
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
            func_8002E53C(&req, D_shelter_b2_laboratory_80182134);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CE);
            Text_Strcat(p, D_shelter_b2_laboratory_80182178);
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
            func_8002E53C(&req, D_shelter_b2_laboratory_8018213C);
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
            Text_Strcat(p, D_shelter_b2_laboratory_80182180);
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
            func_8002E53C(&req, D_shelter_b2_laboratory_80182148);
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
            Text_Strcat(p, D_shelter_b2_laboratory_80182180);
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
            func_8002E53C(&req, D_shelter_b2_laboratory_80182160);
            Text_ItoaUnsigned(p, Mc_SaveData.clearCount);
            Text_Strcat(p, D_shelter_b2_laboratory_80182178);
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
            func_8002E53C(&req, D_shelter_b2_laboratory_80182168);
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
            func_8002E53C(&req, D_shelter_b2_laboratory_80182170);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data panel.
const char D_shelter_b2_laboratory_8017D610[] = "Play Data";

/// Drawn in place of a percentage for a row holding every recorded use.
const u8 D_shelter_b2_laboratory_8017D61C[] = "100.0%";

/// Draws one row of an item-usage panel from the `RoomItemUsage` block in the
/// owning task's work area: the item's name, its share of all recorded uses as
/// a percentage with two decimals, and a gauge scaled by the row's
/// `barWidths` entry. Highlighting the row previews the item; pressing the
/// detail button on the selected row opens the item's detail window.
void func_shelter_b2_laboratory_8017DEE8(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_shelter_b2_laboratory_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_shelter_b2_laboratory_80182180);
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

/// Builds the "Play Data" item-usage panel's three parallel arrays from the
/// save's per-item use counters (`Mc_SaveData.weaponUseCounts`, ids 0x80-0x9F).
///
/// Every id whose name is non-empty (a leading 0 or 0xA marks an unused row)
/// and whose counter is non-zero is marked seen and appended to `itemIds`,
/// while the counters are summed. The ids are then insertion-sorted by use
/// count, most-used first. Finally each row gets `percents` - its share of all
/// recorded uses in hundredths of a percent, rounded - and `barWidths`, its
/// counter as a 12-bit fraction of the top row's. Both are scaled down by
/// halving until the top counter fits in 17 bits, so the multiply and the
/// shift cannot overflow.
void func_shelter_b2_laboratory_8017E2E4(UiList* list, UiObject* obj)
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

/// Parasite Energy counterpart of `func_shelter_b2_laboratory_8017E2E4`: fills
/// the "Play Data" PE-usage panel's `RoomPeUsage` block from the save's
/// per-slot use counters.
///
/// Each of the twelve Parasite Energy slots owns three consecutive ids starting
/// at 0xF, one per level, so slot `i` at level `Mc_SaveData.attachLevels[i]`
/// prints as `i * 3 + 0xF + level - 1` (a slot the player has never levelled
/// keeps the base id). Every slot with a non-zero counter in
/// `Mc_SaveData.attachUseCounts` - which really runs twelve entries wide, past the
/// seven the struct names - is appended and its counter summed. The ids are
/// then insertion-sorted by use count, most-used first, and each row gets
/// `percents`, its share of all recorded uses in hundredths of a percent, and
/// `barWidths`, its counter as a 12-bit fraction of the top row's. Both are
/// scaled down by halving until the top counter fits in 17 bits, so the
/// multiply and the shift cannot overflow.
void func_shelter_b2_laboratory_8017E5E0(UiList* list, UiObject* obj)
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

/// Titles of the usage panel: weapons, then Parasite Energy.
const char D_shelter_b2_laboratory_8017D624[] = "Weapon Data";
const char D_shelter_b2_laboratory_8017D630[] = "PE Data";

/// Task body of the usage panel: `spawnArg1` 0 lists weapons, anything else
/// Parasite Energy. On its first frame it allocates the row block, spawns the
/// row descriptor and fills the list; every frame it updates the list, closes
/// on cancel, and tears down any child window that has finished.
void func_shelter_b2_laboratory_8017E900(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_shelter_b2_laboratory_80182374;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_shelter_b2_laboratory_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_shelter_b2_laboratory_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_shelter_b2_laboratory_80182398, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_shelter_b2_laboratory_8017E2E4(list, obj);
        } else {
            func_shelter_b2_laboratory_8017E5E0(list, obj);
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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory", D_shelter_b2_laboratory_8017D638);

/// Task body of the telephone menu. Until the save has a clear or has reached
/// demo scene 1 it spawns `D_800611E4` in place of the list; otherwise it lays
/// out and updates the list. When the first child window finishes, the menu
/// opens the item prompt its selection picks, or closes.
void func_shelter_b2_laboratory_8017EAB4(Task* task)
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
    list          = &D_shelter_b2_laboratory_801823FC;
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
        /* The literal carries its trailing "\0\1" - the room's rodata has
         * those two bytes right after the string and nothing else claims them. */
        Ui_DrawText((UiPanel*)obj, D_shelter_b2_laboratory_8017D638);
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

/// Task body of a prompt window: on its first frame it becomes the UI holder and
/// installs `func_shelter_b2_laboratory_8017F49C` as its exit callback; every
/// frame it draws the prompt lines.
void func_shelter_b2_laboratory_8017EDAC(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_shelter_b2_laboratory_8017F49C;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void func_shelter_b2_laboratory_8017EE08(u8* str, s32 decimals)
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

/// Format `value` as a percentage with `decimals` fractional digits into `buf`:
/// print the integer with at least `decimals + 1` digits when it is small enough
/// (so "5" with two decimals becomes "0.05"), otherwise print it unpadded, then
/// shift the last `decimals` digits right by one and drop a '.' in front of
/// them. Appends "%" and returns `buf`.
u8* func_shelter_b2_laboratory_8017EE78(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_shelter_b2_laboratory_80182180);
    return buf;
}

/// Task body of the play-data panel: on its first frame it spawns
/// `D_shelter_b2_laboratory_80182398` and lays out the list; every frame it
/// draws the title, updates the list and closes on cancel.
void func_shelter_b2_laboratory_8017EF6C(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b2_laboratory_8018234C;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_shelter_b2_laboratory_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b2_laboratory_80182398, 0, 0, 1, obj);
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

/// Queues a gouraud-shaded rectangle into the current OT one slot past the
/// panel's draw order. Origin is `field_20`/`field_22` plus (`arg1`, `arg2`);
/// `arg3`/`arg4` are width and height. Left vertices take `arg5`, right vertices
/// take `arg6`. A zero color or width < 2 draws nothing.
void func_shelter_b2_laboratory_8017F05C(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

/// The telephone menu's "Save" row: confirmed while the CD is idle, it spawns
/// `D_800611E4` and moves the owning task to state 1.
void func_shelter_b2_laboratory_8017F160(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b2_laboratory_80182100, prompt->field_1C, 1, 0);
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

/// The telephone menu's "Play Data" row: confirmed, it opens
/// `D_shelter_b2_laboratory_801823B4` and moves the owning task to state 2.
void func_shelter_b2_laboratory_8017F244(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b2_laboratory_80182108, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b2_laboratory_801823B4, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "Weapon Data" row: confirmed, it opens the usage panel
/// `D_shelter_b2_laboratory_801823D0` for weapons and moves the owning task to
/// state 2.
void func_shelter_b2_laboratory_8017F30C(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b2_laboratory_80182114, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b2_laboratory_801823D0, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "PE Data" row: confirmed, it opens the usage panel
/// `D_shelter_b2_laboratory_801823D0` for Parasite Energy and moves the owning
/// task to state 2.
void func_shelter_b2_laboratory_8017F3D4(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b2_laboratory_80182120, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b2_laboratory_801823D0, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
void func_shelter_b2_laboratory_8017F49C(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

void func_shelter_b2_laboratory_8017F4D8(Task* task)
{
    s32              out;
    s32              var_a0;
    s32              var_a1;
    s32              temp;
    RoomCutsceneRec* p;

    p = task->spawnArg2;
    switch (task->state) {
        case 0:
            D_shelter_b2_laboratory_801864A4 = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (p->field_0 > 0) {
                D_80115694               = Mc_SaveData.at4.loc.view;
                Mc_SaveData.at4.loc.view = p->field_0;
            } else {
                D_80115694 = -p->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (p->field_4 != 0) {
                SndEvt_EnqueueType6(p->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (p->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(p->field_3);
                var_a0 = p->field_14;
                var_a1 = 0;
                if (var_a0 == 0) {
                    var_a0 = 0x3C0;
                } else {
                    var_a1 = p->field_16;
                }
                func_800E6D4C(var_a0, var_a1);
            }
            if (p->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_shelter_b2_laboratory_801864A4 = Task_SpawnFromTable(D_shelter_b2_laboratory_80182A08, 1, 0, p->field_10);
            Gp_StartCapSlot(p->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(p->field_10, 1);
                taskKill(D_shelter_b2_laboratory_801864A4);
                task->state++;
            } else if (Task_PollKill(D_shelter_b2_laboratory_801864A4, &out) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (p->field_2 == 0) {
                SndEvt_EnqueueType6(p->field_C, 0, 0);
            }
            temp = GameFlag_GetNibble(0x7A);
            if (temp > 0) {
                if (temp >= 5) {
                    if (temp == 5) {
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
            if (p->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(p->field_1, 0);
            }
            if ((GameFlag_GetNibble(0x7A) == 1) && (GameFlag_GetNibble(0) == 2)) {
                GameFlag_SetNibble(0, 3);
                GameFlag_SetNibble(0xE, 4);
                if ((GP_LOC_WORD(Mc_SaveData.at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 1, 0, 0)) {
                    Gp_ApplyAreaRecs(D_80188888);
                    func_800E3FAC(0xA2, 5);
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1);
                    task->state++;
                }
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
            Mc_SaveData.at4.loc.view = D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(p->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (p->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
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
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

/// Handles a request to leave through a flag-gated exit. When the flag named
/// by `req->flagId` (negated: must be clear) is already in the wanted state,
/// returns 1. Otherwise, if the item `req->itemId` has been collected (or none
/// is needed), sets the flag, records `msg` and `req`, spawns the transition
/// task and returns 2; if the item is missing, runs cap command `req->field_4`
/// and returns 0. The spawn and the cap command happen only when
/// `msg->field_5` is 0.
s32 func_shelter_b2_laboratory_8017FA44(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                             = req->flagId;
    D_shelter_b2_laboratory_801864B4 = 0;
    neg                              = flag < 0;
    got                              = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_shelter_b2_laboratory_801864AC = *msg;
                D_shelter_b2_laboratory_8018652C = *req;
                id                               = req->flagId;
                mode                             = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b2_laboratory_80182A2C, 0, 0, 0);
                D_shelter_b2_laboratory_801864B4 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// Transition task: runs cap command `field_0` of the recorded request, plays
/// its two voice events `field_8` and `field_C` in turn (each optional) and
/// waits for them, then stops the sound, points the save's location at the
/// recorded message's destination and spawns the area-change task.
void func_shelter_b2_laboratory_8017FBA8(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b2_laboratory_8018652C.field_0);
            if (D_shelter_b2_laboratory_8018652C.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_laboratory_8018652C.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b2_laboratory_8018652C.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b2_laboratory_8018652C.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_laboratory_8018652C.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b2_laboratory_8018652C.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_shelter_b2_laboratory_801864AC.msgId;
            Mc_SaveData.at4.loc.warp  = D_shelter_b2_laboratory_801864AC.field_2;
            Mc_SaveData.at4.loc.room  = (u8)D_shelter_b2_laboratory_801864AC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b2_laboratory_8017FD18(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        D_shelter_b2_laboratory_801864B8 = 0;
        if (GameFlag_GetNibble(0xD0) == 2) {
            func_shelter_b2_laboratory_801820F4(0);
            GameFlag_SetNibble(0xD0, 3);
            GameFlag_SetNibble(0xB3, 1);
            GameFlag_SetNibble(0x1C2, 0);
            GameFlag_SetNibble(0x17, 1);
            GameFlag_SetNibble(0x18, 1);
            if (GameFlag_GetNibble(0x83) != 0) {
                func_800E3FAC(0xA2, 0x28);
            } else {
                func_800E3FAC(0xA2, 0x29);
            }
            D_shelter_b2_laboratory_801864BC.field_0  = 0xD;
            D_shelter_b2_laboratory_801864BC.field_1  = 4;
            D_shelter_b2_laboratory_801864BC.field_3  = 3;
            D_shelter_b2_laboratory_801864BC.field_2  = 1;
            D_shelter_b2_laboratory_801864BC.field_14 = 0x180;
            D_shelter_b2_laboratory_801864BC.field_16 = 0x100;
            D_shelter_b2_laboratory_801864BC.field_4  = 0x541F0005;
            D_shelter_b2_laboratory_801864BC.field_8  = 0x541F0008;
            D_shelter_b2_laboratory_801864BC.field_10 = 0x541F0006;
            D_shelter_b2_laboratory_801864BC.field_C  = 0x541F0007;
            Task_SpawnFromTable(D_shelter_b2_laboratory_80182A08, 0, 1, (s32)&D_shelter_b2_laboratory_801864BC);
            Task_SpawnFromTable(D_shelter_b2_laboratory_80182A6C, 2, 0, 0);
        } else {
            D_shelter_b2_laboratory_801864BC.field_0  = 0xD;
            D_shelter_b2_laboratory_801864BC.field_1  = 1;
            D_shelter_b2_laboratory_801864BC.field_3  = 2;
            D_shelter_b2_laboratory_801864BC.field_2  = 0;
            D_shelter_b2_laboratory_801864BC.field_14 = 0;
            D_shelter_b2_laboratory_801864BC.field_4  = 0x541F0005;
            D_shelter_b2_laboratory_801864BC.field_8  = 0x541F0008;
            D_shelter_b2_laboratory_801864BC.field_10 = 0x541F0006;
            D_shelter_b2_laboratory_801864BC.field_C  = 0x541F0007;
            Task_SpawnFromTable(D_shelter_b2_laboratory_80182A08, 0, 8, (s32)&D_shelter_b2_laboratory_801864BC);
        }
    }
    return 0;
}

/// States of the room's message task, run by
/// `func_shelter_b2_laboratory_801804A4`: install the message table, idle, die.
const TaskFuncTable3 D_shelter_b2_laboratory_8017D6BC = {
    {
        func_shelter_b2_laboratory_80180450,
        func_shelter_b2_laboratory_80180494,
        taskKill,
    },
};

void func_shelter_b2_laboratory_8017FEB8(Task* arg0)
{
    s8  pan;
    s8  depth;
    s32 vol;

    D_shelter_b2_laboratory_801864DC.coord.t[0] = 0xC1C;
    D_shelter_b2_laboratory_801864DC.coord.t[1] = -0x5DC;
    D_shelter_b2_laboratory_801864DC.coord.t[2] = -0xC80;
    D_shelter_b2_laboratory_801864DC.sub        = &gGfxViewCoord;
    D_shelter_b2_laboratory_801864DC.flg        = 0;
    Gp_UpdateCoord(&D_shelter_b2_laboratory_801864DC);
    pan   = Gp_GetObjPan(&D_shelter_b2_laboratory_801864DC);
    depth = gpGetObjDepth(&D_shelter_b2_laboratory_801864DC);
    switch (arg0->state) {
        case 0:
            SndEvt_EnqueueType6(0x541F000E, pan, depth);
            arg0->state++;
            break;
        case 1:
            if (D_shelter_b2_laboratory_801864B8 == 0) {
                SndEvt_EnqueueType7(0x541F000E, 1);
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
            vol = 0x7F - D_shelter_b2_laboratory_80182A90[gGameSession->at4.loc.view] * 0x7F / 100;
            if (vol >= 0x80) {
                vol = 0x7F;
            }
            SndEvt_EnqueueTypeA(0x541F000E, pan, (s8)vol);
            arg0->state = 1;
            break;
    }
}

/// Plays the sound event passed in `spawnArg2` on frames 0 and 0x50 of the
/// task's life and kills the task at frame 0x78.
void func_shelter_b2_laboratory_80180064(Task* task)
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

s32 func_shelter_b2_laboratory_801800F4(void)
{
    return 0;
}

s32 func_shelter_b2_laboratory_801800FC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    func_80179A04(in, out);
    if (GameFlag_GetNibble(0xD0) == 2) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(5);
        }
        return 2;
    }
    if (in->msgId != 0x21) {
        return 1;
    }
    req.field_0 = 1;
    req.field_4 = 1;
    req.field_8 = 0x541F0014;
    req.field_C = 0x541F0003;
    req.flagId  = 0xB1;
    req.itemId  = 0;
    return func_shelter_b2_laboratory_8017FA44(&req, out);
}

/// Handler for slot-7 msg `0x13EF` in `D_shelter_b2_laboratory_80182A38`: the
/// directed action on the laboratory console (`field_2` 1). Runs the scripted
/// scene once, then replays cap script `6` on later visits.
s32 func_shelter_b2_laboratory_801801D0(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0x13D) != 0) {
            if (GameFlag_GetNibble(0xD0) < 2) {
                Task_SpawnFromTable(D_shelter_b2_laboratory_80182A6C, 0, 0, 0);
            } else {
                Gp_RunCapCmd1(6);
            }
        } else {
            Gp_RunCapCmd1(0x1E);
            GameFlag_SetNibble(0x13D, 1);
        }
    }
    return 0;
}

s32 func_shelter_b2_laboratory_8018025C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x541F0017, 0, 0);
    }
    return 0;
}

void func_shelter_b2_laboratory_80180290(Task* task)
{
    s32 result;

    switch (task->state) {
        case 0:
            D_shelter_b2_laboratory_80182A68 = Task_SpawnFromTable(&D_80134564, 0, 0, 0);
            task->state                     += 1;
            return;
        case 1:
            if (Task_PollKill(D_shelter_b2_laboratory_80182A68, &result) != 0) {
                D_shelter_b2_laboratory_80182A68 = NULL;
                if (result != 0) {
                    GameFlag_SetNibble(0xD0, 1);
                } else {
                    Gp_MsgPlayerWeapon(1);
                }
                taskKill(task);
            }
            return;
    }
}

void func_shelter_b2_laboratory_80180350(Task* task)
{
    GpSndParam* pair;

    switch (task->state) {
        case 0:
            if (gGameSession->at4.loc.view == 0xD) {
                task->state = 1;
            }
            return;
        case 1:
            Mc_SaveData.sceneEvent = 0xE;
            pair                   = (GpSndParam*)&D_8007A39C;
            pair->field_0          = 0;
            pair->field_2          = 0;
            Task_SpawnFromTable(&D_80062774, 0, 0, 0);
            task->state++;
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                Gp_RunCapCmd1(GameFlag_GetNibble(0x83) != 0 ? 0x24 : 0x23);
                taskKill(task);
            }
            return;
    }
}

/// Installs `D_shelter_b2_laboratory_80182A38` as the task's message table,
/// registers the task in pointer slot 7 and steps it on one state.
void func_shelter_b2_laboratory_80180450(Task* task)
{
    task->msgTable = D_shelter_b2_laboratory_80182A38;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Idle state of the room's message task: does nothing. The unused local
/// reproduces the original's stack frame.
void func_shelter_b2_laboratory_80180494(Task* task)
{
    char pad[0x10];
}

/// Runs the handler for the task's current state, from a local copy of
/// `D_shelter_b2_laboratory_8017D6BC`.
void func_shelter_b2_laboratory_801804A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_laboratory_8017D6BC;
    sp.funcs[task->state](task);
}

void func_shelter_b2_laboratory_801804FC(void)
{
    if (D_shelter_b2_laboratory_801864B8 == 0) {
        D_shelter_b2_laboratory_801864B8 = 1;
        func_shelter_b2_laboratory_801820F4(1);
        Task_SpawnFromTable(D_shelter_b2_laboratory_80182A6C, 1, 0, 0);
    }
}

void func_shelter_b2_laboratory_80180548(Task* task)
{
    if (task->state == 0) {
        D_shelter_b2_laboratory_80186540 = 0;
        task->state                      = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[4], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[6], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[8], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[10], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[26], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            break;
        case 3:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x444);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[2], 0x180, 0x444);
            break;
        case 4:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[0], 0x200, 0x444);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[4], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[12], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[14], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[16], 0x200, 0x124);
            break;
        case 5:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[4], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[6], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[8], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[10], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[12], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[14], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[26], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[42], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(D_shelter_b2_laboratory_80182C00, 0x60, 0x80);
            }
            break;
        case 6:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[0], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[2], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[4], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[6], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[8], 0x180, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[20], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[22], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[24], 0x200, 0x124);
            break;
        case 7:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[0], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[12], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[14], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[16], 0x200, 0x124);
            break;
        case 8:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[0], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[2], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[20], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[22], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[24], 0x200, 0x124);
            break;
        case 9:
            func_shelter_b2_laboratory_80180AB4(D_shelter_b2_laboratory_80182BF0, 0x180, 0x124);
            break;
        case 10:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[0], 0x180, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[2], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[4], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[26], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[40], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AB0[42], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AB0[42], 0x60, 0x80);
            }
            break;
        case 12:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[0], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[2], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[18], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[20], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[22], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[30], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182B00[32], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182B00[32], 0x60, 0x80);
            }
            break;
        case 13:
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_8018176C(D_shelter_b2_laboratory_80182C00, 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_8018176C(D_shelter_b2_laboratory_80182C00, 0x60, 0x80);
            }
            break;
        case 15:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[0], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[4], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[6], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[8], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[16], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182B70[18], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(D_shelter_b2_laboratory_80182C00, 0x60, 0x80);
            }
            break;
    }
}

/// Draws a glowing capsule between the world points `arg0` and `arg0 + 1`,
/// projected through `Gfx_ViewWorldMtx`. Each end is a disc of radius
/// `(s16)arg1 * 64` over its depth; for each 0x400 step across half a turn
/// from the screen-space angle between the ends, one gouraud `POLY_G4` wedge
/// is queued at each end and one band joins them. The lit vertices, at the
/// centres, take the colour packed in `arg2` (one nibble per channel),
/// flickering with the animation frame. Nothing is drawn when either
/// projection flags an error.
void func_shelter_b2_laboratory_80180AB4(SVECTOR* arg0, s32 arg1, s32 arg2)
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

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
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
            blend  = ((u8)ds->animFrame & 1) * 8;
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
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre, with an on-screen
/// radius of `(s16)arg2 * 32 / otz`. The lit vertex pulses on green and blue at
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78`.
void func_shelter_b2_laboratory_801812F8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*           head;
    _DrawScratch* block;
    POLY_G4*      prim;
    LINE_G3*      line;
    s32           sine;
    s32           pulse;
    s32           radius;
    s32           i;
    s32           t1;
    s32           t2;
    s32           twice;
    u16           sx;
    u16           sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x18);
        SOFT_TOUCH_REG(tmp);
        block = (_DrawScratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((_DrawScratch*)(head - 0x18))->sx);
    gte_stflg(&((_DrawScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((_DrawScratch*)(head - 0x18))->otz;
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
    SCRATCH_POP_BYTES(0x18);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges around the projected centre. `arg2` is a signed
/// half-extent; on-screen radii are `(s16)arg2 * 64 / otz` (outer) and
/// `(s16)arg2 * 8 / otz` (inner). `arg1` scales `gDisplayState.animFrame` into
/// `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
void func_shelter_b2_laboratory_8018176C(SVECTOR* arg0, s32 arg1, s32 arg2)
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
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw05Scratch*)tmp;
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

void func_shelter_b2_laboratory_801820F4(s16 arg0)
{
    D_shelter_b2_laboratory_80186540 = arg0;
}
