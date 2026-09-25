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
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/unknown_syms.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Block the room's glow and flare tasks carve off `G_SCRATCH_HEAD` for one
/// frame. `vec` is the task coordinate's world translation, projected through
/// `GsWSMATRIX` into `sx` / `sy` with `otz` as its depth. `radius` and
/// `radius2` are two screen radii scaled by the inverse of that depth; the glow
/// computes `radius2` without reading it.
typedef struct {
    s32     otz;
    s32     radius;
    s32     radius2;
    SVECTOR vec;
    s16     sx;
    s16     sy;
} AcropolisFireEscapeGlowScratch;

STATIC_ASSERT_SIZEOF(AcropolisFireEscapeGlowScratch, 0x18);

extern UiObjectDesc   D_800611E4;
extern UiObject*      D_80067634;
extern s32            D_80070F70;
extern u8             D_80071086;
extern u16            D_80072174;
extern s8             D_80072176;
extern s8             D_8007272D;
extern u16            D_80072834;
extern u16            D_80072836;
extern u8             D_80072A93;
extern s32            D_80072A94;
extern s32            D_80072A98;
extern UiObjectDesc   D_8010EFA0;
extern s16            D_80114D08;
extern GpAreaApplyRec D_80188888[];

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `Mc_SaveData`.
extern s8 D_8007217B;

/// View saved when the cutscene starts and restored when it ends.
extern s32 D_80115694;

/// Title of the telephone menu. The bytes after its terminator are not zero,
/// so it stays assembly.
extern const char D_acropolis_fire_escape_8017D638[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
extern u8 D_acropolis_fire_escape_801819F8[];
extern u8 D_acropolis_fire_escape_80181A00[];
extern u8 D_acropolis_fire_escape_80181A0C[];
extern u8 D_acropolis_fire_escape_80181A18[];

/// Row captions of the play-data panel, one per row.
extern u8 D_acropolis_fire_escape_80181A20[];
extern u8 D_acropolis_fire_escape_80181A50[];
extern u8 D_acropolis_fire_escape_80181A28[];
extern u8 D_acropolis_fire_escape_80181A2C[];
extern u8 D_acropolis_fire_escape_80181A34[];
extern u8 D_acropolis_fire_escape_80181A40[];
extern u8 D_acropolis_fire_escape_80181A58[];
extern u8 D_acropolis_fire_escape_80181A60[];
extern u8 D_acropolis_fire_escape_80181A68[];

/// Suffix appended after a plain count on rows 1, 2, 3 and 6 of the play-data
/// panel.
extern u8 D_acropolis_fire_escape_80181A70[];

/// Suffix appended after a percentage.
extern u8 D_acropolis_fire_escape_80181A78[];

/// Help strings handed to the UI holder while the cursor rests on a row of the
/// play-data panel, one per row.
extern u8 D_acropolis_fire_escape_80181A7C[];
extern u8 D_acropolis_fire_escape_80181AA8[];
extern u8 D_acropolis_fire_escape_80181ACC[];
extern u8 D_acropolis_fire_escape_80181AFC[];
extern u8 D_acropolis_fire_escape_80181B30[];
extern u8 D_acropolis_fire_escape_80181B64[];
extern u8 D_acropolis_fire_escape_80181B9C[];
extern u8 D_acropolis_fire_escape_80181BD0[];
extern u8 D_acropolis_fire_escape_80181C08[];

/// Lists of the play-data panel, the usage panel and the telephone menu; the
/// row descriptor both panels spawn; and the play-data and usage panels the
/// telephone menu's rows open.
extern UiList       D_acropolis_fire_escape_80181C44;
extern UiList       D_acropolis_fire_escape_80181C6C;
extern UiObjectDesc D_acropolis_fire_escape_80181C90;
extern UiObjectDesc D_acropolis_fire_escape_80181CAC;
extern UiObjectDesc D_acropolis_fire_escape_80181CC8;
extern UiList       D_acropolis_fire_escape_80181CF4;

/// Task table the cutscene (entry 0) and its sound task (entry 1) are spawned
/// from.
extern TaskDesc D_acropolis_fire_escape_80181D18[];

/// Message table of the room's message task.
extern GpMsgEntry D_acropolis_fire_escape_80181D3C[];

/// Task table holding the room's ambient-sound task.
extern TaskDesc D_acropolis_fire_escape_80181D64[];

/// Circle table the glow and flare tasks read their wedge corners from, in
/// 4.12 fixed point: entry `i` is a corner's y and entry `i + 4` its x.
extern s16 D_acropolis_fire_escape_80181D7C[];

/// Object whose `field_4A` bit 0x40 the message task clears each frame while
/// the slot-4 task does not answer message 0x7D6.
extern GpObj4A D_acropolis_fire_escape_801826A8;

/// Level the ambient-sound task last set for sound event 0x510F0005.
extern s32 D_acropolis_fire_escape_80183040;

/// The cutscene's sound task, killed when the scene is skipped.
extern Task* D_acropolis_fire_escape_80183044;

/// Parameters of the cutscene the 0x13F0 message handler starts.
extern RoomCutsceneRec D_acropolis_fire_escape_80183048;

void func_acropolis_fire_escape_8017F450(Task* task);
void func_acropolis_fire_escape_8017FE50(Task* task);
void func_acropolis_fire_escape_8017FECC(Task* task);

/// Draws one row of the play-data panel, the row picked by
/// `DialogPrompt::field_8`: a caption followed by a value - play time, one of
/// several counters with a unit suffix, or a percentage kept in hundredths
/// whose decimal point is inserted by hand (row 5 also draws a gauge and takes
/// an extra line). While the cursor is on the row its help string is shown.
void func_acropolis_fire_escape_8017D6D0(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_acropolis_fire_escape_80181A7C,
                D_acropolis_fire_escape_80181AA8,
                D_acropolis_fire_escape_80181ACC,
                D_acropolis_fire_escape_80181AFC,
                D_acropolis_fire_escape_80181B30,
                D_acropolis_fire_escape_80181B64,
                D_acropolis_fire_escape_80181B9C,
                D_acropolis_fire_escape_80181BD0,
                D_acropolis_fire_escape_80181C08,
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
            func_8002E53C(&req, D_acropolis_fire_escape_80181A20);
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
            func_8002E53C(&req, D_acropolis_fire_escape_80181A50);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_acropolis_fire_escape_80181A70);
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
            func_8002E53C(&req, D_acropolis_fire_escape_80181A28);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_acropolis_fire_escape_80181A70);
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
            func_8002E53C(&req, D_acropolis_fire_escape_80181A2C);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_acropolis_fire_escape_80181A70);
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
            func_8002E53C(&req, D_acropolis_fire_escape_80181A34);
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
            Text_Strcat(p, D_acropolis_fire_escape_80181A78);
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
            func_8002E53C(&req, D_acropolis_fire_escape_80181A40);
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
            Text_Strcat(p, D_acropolis_fire_escape_80181A78);
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
            func_8002E53C(&req, D_acropolis_fire_escape_80181A58);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_acropolis_fire_escape_80181A70);
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
            func_8002E53C(&req, D_acropolis_fire_escape_80181A60);
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
            func_8002E53C(&req, D_acropolis_fire_escape_80181A68);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data panel.
const char D_acropolis_fire_escape_8017D610[] = "Play Data";

/// Drawn in place of a percentage for a row holding every recorded use.
const u8 D_acropolis_fire_escape_8017D61C[] = "100.0%";

/// Draws one row of an item-usage panel from the `RoomItemUsage` block in the
/// owning task's work area: the item's name, its share of all recorded uses as
/// a percentage with two decimals, and a gauge scaled by the row's
/// `barWidths` entry. Highlighting the row previews the item; pressing the
/// detail button on the selected row opens the item's detail window.
void func_acropolis_fire_escape_8017DE9C(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_acropolis_fire_escape_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_acropolis_fire_escape_80181A78);
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
void func_acropolis_fire_escape_8017E298(UiList* list, UiObject* obj)
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

/// Parasite Energy counterpart of `func_acropolis_fire_escape_8017E298`: fills
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
void func_acropolis_fire_escape_8017E594(UiList* list, UiObject* obj)
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
const char D_acropolis_fire_escape_8017D624[] = "Weapon Data";
const char D_acropolis_fire_escape_8017D630[] = "PE Data";

/// Task body of the usage panel: `spawnArg1` 0 lists weapons, anything else
/// Parasite Energy. On its first frame it allocates the row block, spawns the
/// row descriptor and fills the list; every frame it updates the list, closes
/// on cancel, and tears down any child window that has finished.
void func_acropolis_fire_escape_8017E8B4(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_acropolis_fire_escape_80181C6C;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_acropolis_fire_escape_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_acropolis_fire_escape_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181C90, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_acropolis_fire_escape_8017E298(list, obj);
        } else {
            func_acropolis_fire_escape_8017E594(list, obj);
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

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", D_acropolis_fire_escape_8017D638);

/// Task body of the telephone menu. Until the save has a clear or has reached
/// demo scene 1 it spawns `D_800611E4` in place of the list; otherwise it lays
/// out and updates the list. When the first child window finishes, the menu
/// opens the item prompt its selection picks, or closes.
void func_acropolis_fire_escape_8017EA68(Task* task)
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
    list          = &D_acropolis_fire_escape_80181CF4;
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
        Ui_DrawText((UiPanel*)obj, D_acropolis_fire_escape_8017D638);
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
/// installs `func_acropolis_fire_escape_8017F450` as its exit callback; every
/// frame it draws the prompt lines.
void func_acropolis_fire_escape_8017ED60(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_acropolis_fire_escape_8017F450;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void func_acropolis_fire_escape_8017EDBC(u8* str, s32 decimals)
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
u8* func_acropolis_fire_escape_8017EE2C(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_acropolis_fire_escape_80181A78);
    return buf;
}

/// Task body of the play-data panel: on its first frame it spawns
/// `D_acropolis_fire_escape_80181C90` and lays out the list; every frame it
/// draws the title, updates the list and closes on cancel.
void func_acropolis_fire_escape_8017EF20(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_acropolis_fire_escape_80181C44;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_acropolis_fire_escape_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181C90, 0, 0, 1, obj);
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
void func_acropolis_fire_escape_8017F010(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

/// The telephone menu's "Save" row: confirmed while the CD is idle, it spawns
/// `D_800611E4` and moves the owning task to state 1.
void func_acropolis_fire_escape_8017F114(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_801819F8, prompt->field_1C, 1, 0);
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

/// The telephone menu's "Play Data" row: confirmed, it opens
/// `D_acropolis_fire_escape_80181CAC` and moves the owning task to state 2.
void func_acropolis_fire_escape_8017F1F8(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_80181A00, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181CAC, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "Weapon Data" row: confirmed, it opens the usage panel
/// `D_acropolis_fire_escape_80181CC8` for weapons and moves the owning task to
/// state 2.
void func_acropolis_fire_escape_8017F2C0(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_80181A0C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181CC8, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "PE Data" row: confirmed, it opens the usage panel
/// `D_acropolis_fire_escape_80181CC8` for Parasite Energy and moves the owning
/// task to state 2.
void func_acropolis_fire_escape_8017F388(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_80181A18, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181CC8, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
void func_acropolis_fire_escape_8017F450(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// Task body of the room's cutscene, driven by the
/// `RoomCutsceneRec` in `spawnArg2`. It holds both characters'
/// weapons, hides the HUD, forces the scripted view and loads the CAP file,
/// then starts the scene's CAP slot together with its sound task (entry 1 of
/// `D_acropolis_fire_escape_80181D18`); confirm or cancel skips the scene.
/// Afterwards it runs the follow-up CAP command (slot 1 picks it from game
/// flag 0x155), advances the story flags the scene settles, and restores the
/// view, weapons and HUD. While flag 0x155 is 0xE and flag 3 is clear, the end
/// loops through states 20-23 instead, running the command the CAP event key
/// selects until the key is neither 0xB nor 0xC.
void func_acropolis_fire_escape_8017F48C(Task* task)
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
            D_acropolis_fire_escape_80183044 = NULL;
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
            D_acropolis_fire_escape_80183044 =
                Task_SpawnFromTable(D_acropolis_fire_escape_80181D18, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_acropolis_fire_escape_80183044);
                task->state++;
            } else if (Task_PollKill(D_acropolis_fire_escape_80183044, &poll) != 0) {
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
                if ((*(u32*)&Mc_SaveData.at4.loc & 0xFFFF0000) == 0x01010000) {
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
            if (D_8007217B == 1) {
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

/// States of the room's message task, run by `func_acropolis_fire_escape_8017FF24`:
/// install the message table and spawn the ambient-sound task, run the
/// per-frame check, die.
const TaskFuncTable3 D_acropolis_fire_escape_8017D6A4 = {
    {
        func_acropolis_fire_escape_8017FE50,
        func_acropolis_fire_escape_8017FECC,
        taskKill,
    },
};

/// The `0x13F0` message handler of `D_acropolis_fire_escape_80181D3C`.
/// Event 4 runs CAP command 0xB the first time (setting game flag 0x16A), and
/// afterwards starts the room's cutscene in view 9 with CAP slot and file 1.
/// Event 3 raises flag 0x155 to at least 6 and starts CAP 3. Event 1 starts
/// CAP 9 when the slot-4 task answers message 0x7D6, CAP 1 otherwise.
s32 func_acropolis_fire_escape_8017F9F8(Task* task, s32 msgId, s32 event, s32 arg3)
{
    Task* slot;
    s32   cap;
    s32   result;

    if (event == 4) {
        if (GameFlag_GetNibble(0x16A) == 0) {
            GameFlag_SetNibble(0x16A, 1);
            Gp_RunCapCmd1(0xB);
            return 0;
        }
        D_acropolis_fire_escape_80183048.field_0  = 9;
        D_acropolis_fire_escape_80183048.field_1  = 1;
        D_acropolis_fire_escape_80183048.field_3  = 1;
        D_acropolis_fire_escape_80183048.field_2  = 0;
        D_acropolis_fire_escape_80183048.field_4  = 0x510F0001;
        D_acropolis_fire_escape_80183048.field_8  = 0x510F0004;
        D_acropolis_fire_escape_80183048.field_10 = 0x510F0007;
        D_acropolis_fire_escape_80183048.field_C  = 0x510F0008;
        Task_SpawnFromTable(D_acropolis_fire_escape_80181D18, 0, 3, (s32)&D_acropolis_fire_escape_80183048);
    }
    if (event == 3) {
        if (GameFlag_GetNibble(0x155) < 6) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 6);
        }
        Gp_SpawnIfCapIdle(3, 1);
        func_800E3FAC(0xA2, 7);
    }
    if (event == 1) {
        slot = (Task*)Gp_LookupSlot4(0);
        cap  = 1;
        if (slot != NULL) {
            result = Gp_DispatchMsg(slot, 0x7D6, 0, 0);
            cap    = 9;
            if (result == 0) {
                cap = 1;
            }
        }
        Gp_SpawnIfCapIdle(cap, 1);
    }
    return 0;
}

/// Task body of the room's ambient sound. Each frame it picks a level for the
/// looping sound event 0x510F0005 from the current view (views 2-5 and 8 hear
/// it, the rest silence it) and, when the level changes, starts the sound,
/// fades it out or retunes it. Entering view 8 while the save's scene event is
/// 5 also advances it to 7 and spawns `D_80062774`.
void func_acropolis_fire_escape_8017FB40(Task* task)
{
    GpSndParam* pair;
    s32         vol;
    s32         prev;

    switch (task->state) {
        case 0:
            D_acropolis_fire_escape_80183040 = 0;
            task->state                      = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    switch ((u8)gGameSession->at4.loc.view) {
        case 8:
            vol = 0x64;
            if (Mc_SaveData.sceneEvent == 5) {
                Mc_SaveData.sceneEvent = 7;
                pair                   = (GpSndParam*)&D_8007A39C;
                pair->field_0          = 1;
                pair->field_2          = 1;
                Task_SpawnFromTable(&D_80062774, 0, 0, 0);
                gGameSession->flowFlags = 0;
            }
            break;
        case 2:
        case 3:
            vol = 0x1E;
            break;
        case 4:
        case 5:
            vol = 0xF;
            break;
        default:
            vol = 0;
            break;
    }

    prev = D_acropolis_fire_escape_80183040;
    if (vol == prev) {
        return;
    }
    if (prev == 0) {
        SndEvt_EnqueueType6(0x510F0005, 0, (s8)(((0x64 - vol) * 127) / 100));
    } else if (vol == 0) {
        SndEvt_EnqueueType7(0x510F0005, 0x1E);
    } else {
        SndEvt_EnqueueTypeA(0x510F0005, 0, (s8)(((0x64 - vol) * 127) / 100));
    }
    D_acropolis_fire_escape_80183040 = vol;
}

/// Plays the sound event passed in `spawnArg2` on frames 0 and 0x50 of the
/// task's life and kills the task at frame 0x78.
void func_acropolis_fire_escape_8017FD08(Task* task)
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

/// The `0x13EE` message handler of `D_acropolis_fire_escape_80181D3C`: copies
/// the incoming save location onto the outgoing one, fades the ambient sound
/// out when `field_5` is 0, and for location 0xE with `field_5` 0 sets the
/// outgoing `field_3` to 2 when game flag 2 is 3, to 1 otherwise.
s32 func_acropolis_fire_escape_8017FD98(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (src->field_5 == 0) {
        SndEvt_EnqueueType7(0x510F0005, 0xF);
    }
    if (*(u16*)src == 0xE && src->field_5 == 0) {
        if (GameFlag_GetNibble(2) == 3) {
            dst->field_3 = 2;
        } else {
            dst->field_3 = 1;
        }
    }
    return 1;
}

/// The `0x13F1` message handler of `D_acropolis_fire_escape_80181D3C`: ignores
/// the message.
s32 func_acropolis_fire_escape_8017FE40(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The `0x13F2` message handler of `D_acropolis_fire_escape_80181D3C`: ignores
/// the message.
s32 func_acropolis_fire_escape_8017FE48(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// First state of the room's message task: installs the message table, takes
/// pointer slot 7, spawns the ambient-sound task and, when `D_8007272D` is 5,
/// sets the session's flow flags to 8.
void func_acropolis_fire_escape_8017FE50(Task* task)
{
    task->msgTable = D_acropolis_fire_escape_80181D3C;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(D_acropolis_fire_escape_80181D64, 0, 0, 0);
    if (D_8007272D == 5) {
        gGameSession->flowFlags = 8;
    }
    task->state = task->state + 1;
}

/// Per-frame state of the room's message task: clears bit 0x40 of
/// `D_acropolis_fire_escape_801826A8.field_4A` unless the slot-4 task exists
/// and answers message 0x7D6.
void func_acropolis_fire_escape_8017FECC(Task* task)
{
    Task* slot;

    slot = (Task*)Gp_LookupSlot4(0);
    if (slot == NULL || Gp_DispatchMsg(slot, 0x7D6, 0, 0) == 0) {
        D_acropolis_fire_escape_801826A8.field_4A &= 0xBF;
    }
}

/// Runs the room's message task: copies the state table onto the stack and
/// calls the entry for the task's current state.
void func_acropolis_fire_escape_8017FF24(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_fire_escape_8017D6A4;
    sp.funcs[task->state](task);
}

/// Task body of the room's effect emitter. On its first frame it spawns effect
/// 0x6008C at a fixed offset from the task's coordinate; every later frame
/// outside a cutscene it spawns effect 0x6004F at the offset and with the
/// parameters of the current view, for views 3, 6, 8 and 9.
void func_acropolis_fire_escape_8017FF7C(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    switch (task->state) {
        case 0:
            work->move.vx = 0xB58;
            work->move.vy = -0x822;
            work->move.vz = -0xE5;
            Gp_SpawnEff(0x6008C, coord, 0x42000, &work->move);
            task->state = task->state + 1;
            break;
        case 1:
            if (Gp_State1C->eventState < 4) {
                if ((u8)gGameSession->at4.loc.view == 3) {
                    work->move.vx = 0x48F;
                    work->move.vy = -0x391;
                    work->move.vz = 0x686;
                    Gp_SpawnEff(0x6004F, coord, 0x60E, &work->move);
                }
                if ((u8)gGameSession->at4.loc.view == 8) {
                    work->move.vx = 0x48F;
                    work->move.vy = -0x391;
                    work->move.vz = 0x686;
                    Gp_SpawnEff(0x6004F, coord, 0x8000030E, &work->move);
                }
                if ((u8)gGameSession->at4.loc.view == 6) {
                    work->move.vx = -0xC1F;
                    work->move.vy = -0xD10;
                    work->move.vz = 0x8E0;
                    Gp_SpawnEff(0x6004F, coord, 0x10408, &work->move);
                }
                if ((u8)gGameSession->at4.loc.view == 9) {
                    work->move.vx = -0xC1F;
                    work->move.vy = -0xD10;
                    work->move.vz = 0x8E0;
                    Gp_SpawnEff(0x6004F, coord, 0x80010208, &work->move);
                }
            }
            break;
    }
}

/// Draws a flickering glow at the task's coordinate while the scene is not in
/// a cutscene and the camera is on views 2, 3 or 7. The coordinate is
/// projected through `GsWSMATRIX` and nothing is drawn unless its biased depth
/// stays beyond 0x10. Every 32 frames `index` picks one of four flicker
/// modes, which set the brightness `scale` each frame: random 0/0x10/0x20/0x30,
/// the same but changing only on odd frames, a steady 0x30, or random 0/0x10.
/// Rising from 0x10 or less to 0x20 or more plays sound event 0x510F0006 panned
/// to the coordinate. The glow is eight `POLY_G4` wedges around the projected
/// point, taken two steps at a time from the circle table, each lit at the
/// centre and black at the rim; it is drawn three times, at the full radius
/// with half the brightness, at half the radius with the brightness, and at an
/// eighth of the radius with four times the brightness (wrapping in a byte).
/// Every wedge takes the semi-transparent tpage of `Gp_AddTpageShift`.
void func_acropolis_fire_escape_80180154(Task* task)
{
    GpEffWork*                      work;
    GpCoord*                        coord;
    void**                          scratch;
    u8*                             head;
    u8*                             tmp;
    AcropolisFireEscapeGlowScratch* block;
    POLY_G4*                        prim;
    s32                             play;
    s32                             i;
    u16                             level;
    u16                             vx;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    play  = 0;
    if (Gp_State1C->eventState < 4 && ((0x46 >> (gGameSession->at4.loc.view - 1)) & 1)) {
        Gp_UpdateCoord(coord);
        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        vx      = coord->workm.t[0];
        tmp     = head - 0x18;
        SOFT_USE_REG(tmp); /* keeps the carve apart from `block`, so the head store takes the copy */
        block         = (AcropolisFireEscapeGlowScratch*)tmp;
        block->vec.vx = vx;
        block->vec.vy = coord->workm.t[1];
        *scratch      = block;
        block->vec.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps();
        gte_stsxy(&block->sx);
        gte_stszotz(&block->otz);
        block->otz -= 0x20;
        if (block->otz > 0x10) {
            if (!(gDisplayState.animFrame & 0x1F)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                work->index = (Gp_LcgState >> 16) & 3;
            }
            level = work->scale;
            if (work->scale < 0x11) {
                play = 1;
            }
            switch (work->index) {
                case 0:
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    work->scale = (Gp_LcgState >> 16) & 0x30;
                    break;
                case 1:
                    if (gDisplayState.animFrame & 1) {
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        level       = (Gp_LcgState >> 16) & 0x30;
                    }
                    work->scale = level;
                    break;
                case 2:
                    work->scale = 0x30;
                    break;
                case 3:
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    work->scale = (Gp_LcgState >> 16) & 0x10;
                    break;
            }
            if (play && work->scale >= 0x20) {
                SndEvt_EnqueueType6(0x510F0006, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            block->radius  = (((task->spawnArg1 >> 8) & 0xFF) * 0x600) / block->otz;
            block->radius2 = (((task->spawnArg1 >> 8) & 0xFF) * 0xC0) / block->otz;
            for (i = 0; i < 0x10; i += 2) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = (u8*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, work->scale >> 1, work->scale >> 1, work->scale >> 1);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                prim->y0 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i]) >> 12);
                prim->x1 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 12);
                prim->y1 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 12);
                prim->x2 = block->sx;
                prim->y2 = block->sy;
                prim->x3 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 12);
                prim->y3 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift(prim, 1, block->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = (u8*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, work->scale, work->scale, work->scale);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 13);
                prim->y0 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i]) >> 13);
                prim->x1 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 13);
                prim->y1 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 13);
                prim->x2 = block->sx;
                prim->y2 = block->sy;
                prim->x3 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 13);
                prim->y3 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 13);
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift(prim, 1, block->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = (u8*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (u8)work->scale * 4, (u8)work->scale * 4, (u8)work->scale * 4);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 15);
                prim->y0 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i]) >> 15);
                prim->x1 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 15);
                prim->y1 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 15);
                prim->x2 = block->sx;
                prim->y2 = block->sy;
                prim->x3 = block->sx + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 15);
                prim->y3 = block->sy + ((block->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 15);
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift(prim, 1, block->otz);
            }
        }
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Draws a pulsing flare at the task's coordinate, projected through
/// `GsWSMATRIX`, and nothing when its depth is 0x10 or less. The brightness is
/// a triangle wave of `D_80070F70` times the low byte of `spawnArg1`; bit 16
/// tints it cyan instead of red, and bits 8-15 size it, divided by the depth.
/// A negative `spawnArg1` draws a radial glow of `POLY_G4` wedges with four
/// longer rays; otherwise a flat diamond of two `POLY_G4`s, with two crossed
/// `LINE_G3` streaks when bit 28 is set. Every primitive takes the
/// semi-transparent tpage of `Gp_AddTpageShift`. Finally `spawnArg2` goes to
/// `Gp_ReleaseState1CMem`.
void func_acropolis_fire_escape_80180B20(Task* task)
{
    u8*                             head;
    u8*                             raw;
    AcropolisFireEscapeGlowScratch* blk;
    POLY_G4*                        prim;
    LINE_G3*                        line;
    GpCoord*                        coord;
    void*                           mem;
    u16                             vz;
    s32                             i;
    s32                             pulse;
    s32                             level;
    s32                             height;
    s16                             amp;
    s16                             flip;
    s32                             ampSi;
    s32                             ampHalf;
    u8                              red;
    u8                              cyan;
    s32                             z;
    s32                             shift;
    u32                             depth;
    u32                             tag;
    u_long*                         ot;

    coord = task->extra.tmd->coords;
    mem   = task->spawnArg2;
    Gp_UpdateCoord(coord);
    head = SCRATCH_HEAD(void);
    raw  = head - 0x18;
    SOFT_TOUCH_REG(raw);
    blk                = (AcropolisFireEscapeGlowScratch*)raw;
    blk->vec.vx        = (u16)coord->workm.t[0];
    blk->vec.vy        = (u16)coord->workm.t[1];
    vz                 = (u16)coord->workm.t[2];
    SCRATCH_HEAD(void) = blk;
    blk->vec.vz        = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisFireEscapeGlowScratch*)(head - 0x18))->vec);
    gte_rtps();
    gte_stsxy(&((AcropolisFireEscapeGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&blk->otz);
    if (((AcropolisFireEscapeGlowScratch*)(head - 0x18))->otz >= 0x11) {
        pulse  = D_80070F70;
        pulse *= task->spawnArg1 & 0xFF;
        flip   = (task->spawnArg1 >> 16) & 1;
        if (pulse & 0x80) {
            level = ~pulse & 0x7F;
        } else {
            level = pulse & 0x7F;
        }
        amp   = level * 2;
        level = task->spawnArg1;
        if (level < 0) {
            height       = (level >> 8) & 0xFF;
            blk->radius  = (height << 10) / blk->otz;
            blk->radius2 = (((task->spawnArg1 >> 8) & 0xFF) << 7) / blk->otz;
            for (i = 0; i < 0x10; i += 2) {
                ampSi          = amp;
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (ampSi * (flip ^ 1)) >> 1, (flip * ampSi) >> 1, (flip * ampSi) >> 1);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i]) >> 12);
                prim->x1 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, ampSi * (flip ^ 1), flip * ampSi, flip * ampSi);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i]) >> 13);
                prim->x1 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 5]) >> 13);
                prim->y1 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 1]) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 6]) >> 13);
                prim->y3 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 2]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            ampHalf = amp >> 1;
            for (i = 2; i < 0x10; i += 8) {
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    red  = ampHalf * (flip ^ 1);
                    cyan = flip * ampHalf;
                    setRGB2(prim, red, cyan, cyan);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = blk->sx + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i]) >> 12);
                    prim->y0 = blk->sy + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i - 4]) >> 12);
                    prim->x1 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 11);
                    prim->y1 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i]) >> 11);
                    prim->x2 = blk->sx;
                    prim->y2 = blk->sy;
                    prim->x3 = blk->sx + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 8]) >> 12);
                    prim->y3 = blk->sy + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                    shift    = gDisplayState.otDepthShift;
                    depth    = (((u32)blk->otz << shift) >> 2) & 0xFFC;
                    __asm__("" : "+r"(depth) : "r"(shift), "m"(gDisplayState.otDepthShift));
                    setaddr(prim, getaddr((u_long*)(depth + (s32)gGpuCurrentOt)));
                    ot  = (u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt);
                    tag = (*ot & 0xFF000000) | ((u32)prim & 0xFFFFFF);
                    *ot = tag;
                    z   = blk->otz;
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG_USE(z, tag);
                    SOFT_TOUCH_REG_USE(prim, z);
                    Gp_AddTpageShift((P_TAG*)prim, 1, z);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, red, cyan, cyan);
                } while (0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i]) >> 13);
                prim->x1 = blk->sx + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 8]) >> 12);
                prim->y1 = blk->sy + ((blk->radius * D_acropolis_fire_escape_80181D7C[i + 4]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 0xC]) >> 13);
                prim->y3 = blk->sy + ((blk->radius2 * D_acropolis_fire_escape_80181D7C[i + 8]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                z = blk->otz;
                __asm__("" : "+r"(z) : "r"(red), "r"(&D_acropolis_fire_escape_80181D7C[i]));
                __asm__("" : "+r"(prim) : "r"(z), "r"(cyan));
                Gp_AddTpageShift((P_TAG*)prim, 1, z);
            }
        } else {
            blk->radius  = (((level >> 8) & 0xFF) << 9) / blk->otz;
            blk->radius2 = (((task->spawnArg1 >> 8) & 0xFF) << 9) / blk->otz;
            for (i = 0; i < 2; i++) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, amp * (flip ^ 1), flip * amp, flip * amp);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx - blk->radius;
                prim->x1 = prim->x2 = blk->sx;
                prim->x3            = blk->sx + blk->radius;
                prim->y0 = prim->y2 = prim->y3 = blk->sy;
                prim->y1                       = (blk->sy - blk->radius2) + blk->radius2 * (i + i);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            if (task->spawnArg1 & 0x10000000) {
                for (i = 0; i < 2; i++) {
                    line           = (LINE_G3*)gGpuPrimCursor;
                    gGpuPrimCursor = line + 1;
                    setLineG3(line);
                    setRGB0(line, 0, 0, 0);
                    setRGB1(line, amp * (flip ^ 1), flip * amp, flip * amp);
                    setRGB2(line, 0, 0, 0);
                    line->x0 = blk->sx + blk->radius * (i * 3 - 1);
                    line->y0 = blk->sy - blk->radius2 * (i + 1);
                    line->x1 = blk->sx;
                    line->y1 = blk->sy;
                    line->x2 = blk->sx - blk->radius * (i * 3 - 1);
                    line->y2 = blk->sy + blk->radius2 * (i + 1);
                    addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
                }
            }
        }
    }
    SCRATCH_POP_BYTES(0x18);
    Gp_ReleaseState1CMem(mem, task);
}
