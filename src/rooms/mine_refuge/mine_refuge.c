#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
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

/// Scratch block `func_mine_refuge_80180710` takes from `G_SCRATCH_HEAD`.
/// `otz`, `flag` and `sx`/`sy` receive the projection of the glow's centre;
/// `rOuter` and `rInner` are its two on-screen radii, derived from that `otz`.
/// Nothing in the function touches the leading bytes.
typedef struct {
    u8  _pad0[8];
    s32 otz;
    s32 flag;
    s32 rOuter;
    s32 rInner;
    u16 sx;
    u16 sy;
} MineRefugeGlowScratch;

STATIC_ASSERT_SIZEOF(MineRefugeGlowScratch, 0x1C);

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern UiObjectDesc   D_800611E4;
extern u8             D_80062735;
extern UiObject*      D_80067634;
extern u8             D_80071086;
extern u16            D_80072174;
extern s8             D_80072176;
extern u16            D_80072834;
extern u16            D_80072836;
extern u8             D_80072A93;
extern s32            D_80072A94;
extern s32            D_80072A98;
extern UiObjectDesc   D_8010EFA0;
extern s16            D_80114D08;
extern u8             D_80115598;
extern TaskDesc       D_801358D8;
extern GpAreaApplyRec D_80188888[];

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `Mc_SaveData`.
extern s8 D_8007217B;

/// View saved when the cutscene starts and restored when it ends.
extern s32 D_80115694;

/// Title of the telephone menu. The bytes after its terminator are not zero,
/// so it stays assembly.
extern const char D_mine_refuge_8017D638[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
extern u8 D_mine_refuge_80181540[];
extern u8 D_mine_refuge_80181548[];
extern u8 D_mine_refuge_80181554[];
extern u8 D_mine_refuge_80181560[];

/// Row captions of the play-data panel, one per row.
extern u8 D_mine_refuge_80181568[];
extern u8 D_mine_refuge_80181598[];
extern u8 D_mine_refuge_80181570[];
extern u8 D_mine_refuge_80181574[];
extern u8 D_mine_refuge_8018157C[];
extern u8 D_mine_refuge_80181588[];
extern u8 D_mine_refuge_801815A0[];
extern u8 D_mine_refuge_801815A8[];
extern u8 D_mine_refuge_801815B0[];

/// Suffix appended after a plain count on rows 1, 2, 3 and 6 of the play-data
/// panel.
extern u8 D_mine_refuge_801815B8[];

/// Suffix appended after a percentage.
extern u8 D_mine_refuge_801815C0[];

/// Help strings handed to the UI holder while the cursor rests on a row of
/// the play-data panel, one per row.
extern u8 D_mine_refuge_801815C4[];
extern u8 D_mine_refuge_801815F0[];
extern u8 D_mine_refuge_80181614[];
extern u8 D_mine_refuge_80181644[];
extern u8 D_mine_refuge_80181678[];
extern u8 D_mine_refuge_801816AC[];
extern u8 D_mine_refuge_801816E4[];
extern u8 D_mine_refuge_80181718[];
extern u8 D_mine_refuge_80181750[];

/// Lists of the play-data panel, the usage panel and the telephone menu, and
/// the descriptors of the panels they spawn.
extern UiList       D_mine_refuge_8018178C;
extern UiList       D_mine_refuge_801817B4;
extern UiObjectDesc D_mine_refuge_801817D8;
extern UiObjectDesc D_mine_refuge_801817F4;
extern UiObjectDesc D_mine_refuge_80181810;
extern UiList       D_mine_refuge_8018183C;

/// Task table the cutscene and its sound task are spawned from.
extern TaskDesc D_mine_refuge_80181860;

/// Message table of the room's message task.
extern GpMsgEntry D_mine_refuge_80181884[];

/// Task table of the room's two scripted sequences,
/// `func_mine_refuge_8017FA08` and `func_mine_refuge_8017FDBC`.
extern TaskDesc D_mine_refuge_801818B4;

/// World-space anchors of the room's per-view glows: `D8` and `E0` are the two
/// drawn in view 2, `E0` again in view 6, and `E8` the one of views 3-5. `E0`
/// is reached both as `D8[1]` (view 2) and by its own name (view 6), and the
/// two forms are different code - indexing emits `D8+8`, naming emits its own
/// `lui` - so it keeps its own declaration.
extern SVECTOR D_mine_refuge_801818D8[];
extern SVECTOR D_mine_refuge_801818E0;
extern SVECTOR D_mine_refuge_801818E8;

/// The cutscene's sound task, killed when the scene is skipped.
extern Task* D_mine_refuge_80182AD4;

/// Task `func_mine_refuge_8017FA08` spawns from `D_801358D8` and waits on;
/// message 0x13F1 is relayed to it while it exists.
extern Task* D_mine_refuge_80182AD8;

/// View saved when `func_mine_refuge_8017FC2C` forces view 6 for its scene,
/// restored when the scene ends.
extern u8 D_mine_refuge_80182ADC;

/// Parameters of the cutscene `func_mine_refuge_8017FE78` starts.
extern RoomCutsceneRec D_mine_refuge_80182AE0;

void func_mine_refuge_8017F460(Task* task);
void func_mine_refuge_8017FE78(s32 arg0);
void func_mine_refuge_8017FF4C(Task* task);
void func_mine_refuge_8017FFAC(Task* task);

/// Draws one row of the play-data panel, the row picked by
/// `DialogPrompt::field_8`: a caption followed by a value - play time, one of
/// several counters with a unit suffix, or a percentage kept in hundredths
/// whose decimal point is inserted by hand (row 5 also draws a gauge and takes
/// an extra line). While the cursor is on the row its help string is shown.
void func_mine_refuge_8017D6E0(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_mine_refuge_801815C4,
                D_mine_refuge_801815F0,
                D_mine_refuge_80181614,
                D_mine_refuge_80181644,
                D_mine_refuge_80181678,
                D_mine_refuge_801816AC,
                D_mine_refuge_801816E4,
                D_mine_refuge_80181718,
                D_mine_refuge_80181750,
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
            func_8002E53C(&req, D_mine_refuge_80181568);
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
            func_8002E53C(&req, D_mine_refuge_80181598);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_mine_refuge_801815B8);
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
            func_8002E53C(&req, D_mine_refuge_80181570);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_mine_refuge_801815B8);
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
            func_8002E53C(&req, D_mine_refuge_80181574);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_mine_refuge_801815B8);
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
            func_8002E53C(&req, D_mine_refuge_8018157C);
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
            Text_Strcat(p, D_mine_refuge_801815C0);
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
            func_8002E53C(&req, D_mine_refuge_80181588);
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
            Text_Strcat(p, D_mine_refuge_801815C0);
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
            func_8002E53C(&req, D_mine_refuge_801815A0);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_mine_refuge_801815B8);
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
            func_8002E53C(&req, D_mine_refuge_801815A8);
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
            func_8002E53C(&req, D_mine_refuge_801815B0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data panel.
const char D_mine_refuge_8017D610[] = "Play Data";

/// Drawn in place of a percentage for a row holding every recorded use.
const u8 D_mine_refuge_8017D61C[] = "100.0%";

/// Draws one row of an item-usage panel from the `RoomItemUsage` block in the
/// owning task's work area: the item's name, its share of all recorded uses as
/// a percentage with two decimals, and a gauge scaled by the row's
/// `barWidths` entry. Highlighting the row previews the item; pressing the
/// detail button on the selected row opens the item's detail window.
void func_mine_refuge_8017DEAC(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_mine_refuge_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_mine_refuge_801815C0);
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
void func_mine_refuge_8017E2A8(UiList* list, UiObject* obj)
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

/// Parasite Energy counterpart of `func_mine_refuge_8017E2A8`: fills
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
void func_mine_refuge_8017E5A4(UiList* list, UiObject* obj)
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
const char D_mine_refuge_8017D624[] = "Weapon Data";
const char D_mine_refuge_8017D630[] = "PE Data";

/// Task body of the usage panel: `spawnArg1` 0 lists weapons, anything else
/// Parasite Energy. On its first frame it allocates the row block, spawns the
/// row descriptor and fills the list; every frame it updates the list, closes
/// on cancel, and tears down any child window that has finished.
void func_mine_refuge_8017E8C4(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_mine_refuge_801817B4;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_mine_refuge_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_mine_refuge_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_mine_refuge_801817D8, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_mine_refuge_8017E2A8(list, obj);
        } else {
            func_mine_refuge_8017E5A4(list, obj);
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

INCLUDE_RODATA("rooms/nonmatchings/mine_refuge/mine_refuge", D_mine_refuge_8017D638);

/// Task body of the telephone menu. Until the save has a clear or has reached
/// demo scene 1 it spawns `D_800611E4` in place of the list; otherwise it lays
/// out and updates the list. When the first child window finishes, the menu
/// opens the item prompt its selection picks, or closes.
void func_mine_refuge_8017EA78(Task* task)
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
    list          = &D_mine_refuge_8018183C;
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
        Ui_DrawText((UiPanel*)obj, D_mine_refuge_8017D638);
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
/// installs `func_mine_refuge_8017F460` as its exit callback; every
/// frame it draws the prompt lines.
void func_mine_refuge_8017ED70(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_mine_refuge_8017F460;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void func_mine_refuge_8017EDCC(u8* str, s32 decimals)
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
u8* func_mine_refuge_8017EE3C(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_mine_refuge_801815C0);
    return buf;
}

/// Task body of the play-data panel: on its first frame it spawns
/// `D_mine_refuge_801817D8` and lays out the list; every frame it
/// draws the title, updates the list and closes on cancel.
void func_mine_refuge_8017EF30(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_mine_refuge_8018178C;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_mine_refuge_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_mine_refuge_801817D8, 0, 0, 1, obj);
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
void func_mine_refuge_8017F020(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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
void func_mine_refuge_8017F124(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mine_refuge_80181540, prompt->field_1C, 1, 0);
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
/// `D_mine_refuge_801817F4` and moves the owning task to state 2.
void func_mine_refuge_8017F208(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mine_refuge_80181548, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mine_refuge_801817F4, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "Weapon Data" row: confirmed, it opens the usage panel
/// `D_mine_refuge_80181810` for weapons and moves the owning task to
/// state 2.
void func_mine_refuge_8017F2D0(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mine_refuge_80181554, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mine_refuge_80181810, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// The telephone menu's "PE Data" row: confirmed, it opens the usage panel
/// `D_mine_refuge_80181810` for Parasite Energy and moves the owning
/// task to state 2.
void func_mine_refuge_8017F398(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_mine_refuge_80181560, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_mine_refuge_80181810, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
void func_mine_refuge_8017F460(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

void func_mine_refuge_8017F49C(Task* task)
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
            D_mine_refuge_80182AD4 = NULL;
            Gp_MsgPlayerWeapon(0);
            save = &Mc_SaveData;
            if (save->companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (script->field_0 > 0) {
                D_80115694         = save->at4.loc.view;
                save->at4.loc.view = *(u8*)&script->field_0;
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
            D_mine_refuge_80182AD4 =
                Task_SpawnFromTable(&D_mine_refuge_80181860, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_mine_refuge_80182AD4);
                task->state++;
            } else if (Task_PollKill(D_mine_refuge_80182AD4, &poll) != 0) {
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

/// States of the room's message task, run by `func_mine_refuge_8017FFBC`:
/// install the message table, idle, die.
const TaskFuncTable3 D_mine_refuge_8017D6A4 = {
    {
        func_mine_refuge_8017FF4C,
        func_mine_refuge_8017FFAC,
        taskKill,
    },
};

void func_mine_refuge_8017FA08(Task* task)
{
    s32 sp10;

    switch (task->state) {
        case 0:
            if (GameFlag_GetNibble(0x166) == 1) {
                GameFlag_SetNibble(0x166, 2);
                Gp_RunCapCmd1(0xF);
            }
            task->state = task->state + 1;
            return;
        case 2:
            SndEvt_EnqueueType6(0x54060007, 0, 0);
            D_mine_refuge_80182AD8 = Task_SpawnFromTable(&D_801358D8, 0, 0, 0);
            task->state            = task->state + 1;
            return;
        case 3:
            if (Task_PollKill(D_mine_refuge_80182AD8, &sp10) != 0) {
                D_mine_refuge_80182AD8 = NULL;
                task->state            = task->state + 1;
            }
            return;
        case 1:
        case 4:
            task->state = task->state + 1;
            return;
        case 5:
            SndEvt_EnqueueType6(0x54060008, 0, 0);
            taskKill(task);
            break;
    }
}

/// Plays the sound event passed in `spawnArg2` on frames 0 and 0x50 of the
/// task's life and kills the task at frame 0x78.
void func_mine_refuge_8017FB24(Task* task)
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

/// The `0x13F1` message handler of `D_mine_refuge_80181884`: relays the
/// message unchanged to `D_mine_refuge_80182AD8` and returns its answer, or 0
/// while that task does not exist.
s32 func_mine_refuge_8017FBB4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 ret;

    if (D_mine_refuge_80182AD8 == NULL) {
        ret = 0;
    } else {
        ret = Gp_DispatchMsg(D_mine_refuge_80182AD8, msgId, arg2, arg3);
    }
    return ret;
}

/// A handler of the room's message table: copies the incoming record onto the
/// outgoing one, hands both to `func_80179A04` and returns 1.
s32 func_mine_refuge_8017FBE8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

s32 func_mine_refuge_8017FC2C(Task* task, s32 msgId, s32 arg2)
{
    u8 temp_a3;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x12B) != 0) {
            func_mine_refuge_8017FE78(0U);
        } else {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            temp_a3                  = Mc_SaveData.at4.loc.view;
            Mc_SaveData.at4.loc.view = 6U;
            D_mine_refuge_80182ADC   = temp_a3;
            SndEvt_EnqueueType6(0x54060003, 0, 0);
            Gp_RunCapCmd(0xD, 0);
            Task_SpawnFromTable(&D_mine_refuge_801818B4, 1, 0, 0);
        }
    }
    return 0;
}

s32 func_mine_refuge_8017FCD0(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    u8 temp_s0 = arg2->field_2;

    if (temp_s0 == 1) {
        if (GameFlag_GetNibble(0xBB) != temp_s0) {
            GameFlag_SetNibble(0xC4, 0);
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_mine_refuge_801818B4, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(0xA);
        }
    }
    return 0;
}

/// The `0x13F2` message handler of `D_mine_refuge_80181884`: arguments 0xC, 0x63
/// and 0x67 each cue a sound (ids 0x5406000C, 0x5406000F and 0x5406000D),
/// centred and at zero depth. Any other argument is ignored.
s32 func_mine_refuge_8017FD48(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0xC:
            SndEvt_EnqueueType6(0x5406000C, 0, 0);
            break;
        case 0x63:
            SndEvt_EnqueueType6(0x5406000F, 0, 0);
            break;
        case 0x67:
            SndEvt_EnqueueType6(0x5406000D, 0, 0);
            break;
    }
    return 0;
}

void func_mine_refuge_8017FDBC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 5) {
                arg0->state = arg0->state + 1;
                return;
            }
            Mc_SaveData.at4.loc.view = D_mine_refuge_80182ADC;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        case 1:
            GameFlag_SetNibble(0x12B, 1);
            func_mine_refuge_8017FE78(D_mine_refuge_80182ADC);
            break;
        default:
            return;
    }
    taskKill(arg0);
}

/// Fills `D_mine_refuge_80182AE0` and spawns the cutscene task with it. A
/// non-zero `arg0` is the view restored when the scene ends, with no opening
/// sound; zero forces view 6 for the scene and opens with sound 0x54060003.
/// Progress nibble 0x155 picks the scene: when it is 0xF, CAP slot 0xE with no
/// file and CAP command 1 afterwards; otherwise CAP slot 1 from file 1 and
/// command 5 afterwards.
void func_mine_refuge_8017FE78(s32 arg0)
{
    s32 slot;

    if (arg0 != 0) {
        D_mine_refuge_80182AE0.field_4 = 0;
        D_mine_refuge_80182AE0.field_0 = -arg0;
    } else {
        D_mine_refuge_80182AE0.field_0 = 6;
        D_mine_refuge_80182AE0.field_4 = 0x54060003;
    }
    if (GameFlag_GetNibble(0x155) == 0xF) {
        slot                           = 1;
        D_mine_refuge_80182AE0.field_1 = 0xE;
        D_mine_refuge_80182AE0.field_3 = 0;
    } else {
        slot                           = 5;
        D_mine_refuge_80182AE0.field_1 = 1;
        D_mine_refuge_80182AE0.field_3 = 1;
    }
    D_mine_refuge_80182AE0.field_2  = 0;
    D_mine_refuge_80182AE0.field_8  = 0x54060006;
    D_mine_refuge_80182AE0.field_10 = 0x54060004;
    D_mine_refuge_80182AE0.field_C  = 0x54060005;
    Task_SpawnFromTable(&D_mine_refuge_80181860, 0, slot, (s32)&D_mine_refuge_80182AE0);
}

void func_mine_refuge_8017FF4C(Task* arg0)
{
    arg0->msgTable = D_mine_refuge_80181884;
    Game_SetPtrSlot(arg0, 7);
    D_mine_refuge_80182AD8 = NULL;
    D_80062735             = 1;
    arg0->state            = arg0->state + 1;
    D_80115598             = 1;
}

/// Idle state of the room's message task: does nothing.
void func_mine_refuge_8017FFAC(Task* task)
{
    char pad[0x10];
}

/// Runs the handler for the task's current state, from a local copy of
/// `D_mine_refuge_8017D6A4`.
void func_mine_refuge_8017FFBC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_refuge_8017D6A4;
    sp.funcs[task->state](task);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues one semi-transparent `POLY_FT4` sprite
/// centred on the projected point (tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`).
/// `arg1` also picks the 40-texel-wide texture column `(s16)arg1 * 40`, rows
/// 0..0x27. The sprite's on-screen half-extent is `(s16)arg2 * 39 / otz`, and
/// its grey level alternates between 0x20 and 0x30 with the frame counter.
/// A 0x10-byte scratch block is taken from `G_SCRATCH_HEAD` and returned.
void func_mine_refuge_80180014(SVECTOR* arg0, s32 arg1, s32 arg2)
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
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x10;
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre, with an on-screen
/// radius of `(s16)arg2 * 32 / otz`. The lit vertex pulses on green and blue at
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78`. A 0x18-byte scratch block in the
/// `RoomDraw14Scratch` layout is taken from `G_SCRATCH_HEAD` and returned.
void func_mine_refuge_8018029C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw14Scratch* block;
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
        tmp     = (*scratch = head - 0x18);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw14Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / block->otz;
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues a glow of gouraud `POLY_G4` wedges
/// around the projected centre: an eight-wedge disc of radius
/// `(s16)arg2 * 64 / otz`, each wedge paired with a half-radius copy, then four
/// wedges reaching between that radius and an inner one of `(s16)arg2 * 8 /
/// otz`. Only the centre vertex is lit, on green and blue, with a level of
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78` so the glow pulses; the half-radius
/// copies take that level and every other wedge half of it. The scratch block is returned to
/// `G_SCRATCH_HEAD` on exit.
void func_mine_refuge_80180710(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                 scratch;
    u8*                    head;
    MineRefugeGlowScratch* block;
    POLY_G4*               prim;
    s32                    pulse;
    s32                    color;
    s32                    half;
    s32                    size;
    s32                    ang;
    s32                    t;
    s32                    t2;
    s32                    u;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (MineRefugeGlowScratch*)(*scratch = head - 0x1C);

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((MineRefugeGlowScratch*)(head - 0x1C))->sx);
    gte_stflg(&((MineRefugeGlowScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((MineRefugeGlowScratch*)(head - 0x1C))->otz);
        pulse         = rsin(gDisplayState.animFrame * (s16)arg1);
        ang           = 0;
        size          = (s16)arg2;
        block->rOuter = (size * 64) / block->otz;
        block->rInner = (size * 8) / block->otz;
        color         = pulse / 34 + 0x78;
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues three concentric rings of eight
/// gouraud `POLY_G4` wedges around the projected centre. The first ring's
/// radius is `(s16)arg1 * 64 / otz`; each later ring doubles it and halves the
/// centre colour. `arg2` packs three RGB nibbles for the centre vertex, each
/// offset by `(animFrame & 1) << 5` so the glow flickers on alternate frames.
/// Unlike the room's other draws it never returns its 0x10-byte scratch block
/// to `G_SCRATCH_HEAD`.
void func_mine_refuge_80181094(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ring;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    block   = (RoomDraw13Scratch*)(*scratch = head - 0x10);

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ring          = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) << 5;
        packed        = arg2 << 16;
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            ang = 0;
            do {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, r, g, b);
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
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            } while (ang < 0x1000);
            r              = (u8)r >> 1;
            g              = (u8)g >> 1;
            b              = (u8)b >> 1;
            block->radius *= 2;
            ring++;
        } while (ring < 3);
    }
}

/// Draws the room's glows for whichever view is current. View 2 draws a
/// sprite and a diamond; views 3 and 4/5 draw rings at one anchor, but only
/// while progress nibble 0xC3 is 1; view 6 draws a pulsing disc.
void func_mine_refuge_80181454(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_mine_refuge_80180014(&D_mine_refuge_801818D8[0], 1, 0x300);
            func_mine_refuge_8018029C(&D_mine_refuge_801818D8[1], 0x60, 0x40);
            break;
        case 3:
            if (GameFlag_GetNibble(0xC3) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x30, 0xF0);
            }
            break;
        case 4:
        case 5:
            if (GameFlag_GetNibble(0xC3) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x60, 0xD0);
            }
            break;
        case 6:
            func_mine_refuge_80180710(&D_mine_refuge_801818E0, 0x60, 0x80);
            break;
    }
}
