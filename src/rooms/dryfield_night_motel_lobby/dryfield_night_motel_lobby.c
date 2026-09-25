#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
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
#include "rooms/dryfield_night_motel_lobby.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

extern UiObject*    D_80067634;
extern UiObjectDesc D_800611E4;
extern UiObjectDesc D_8010EFA0;

extern s16 D_80114D08;

/// Saved `Mc_SaveData.at4.loc.view` (area id), restored when the cutscene ends.
extern s32 D_80115694;

/// `Mc_SaveData.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `Mc_SaveData` address with case 0.

extern GpAreaApplyRec D_80188888[];

/// Row labels of the play-data statistics panel, one per row
/// `func_dryfield_night_motel_lobby_8017D748` draws.
extern u8 D_dryfield_night_motel_lobby_801824B0[];
extern u8 D_dryfield_night_motel_lobby_801824E0[];
extern u8 D_dryfield_night_motel_lobby_801824B8[];
extern u8 D_dryfield_night_motel_lobby_801824BC[];
extern u8 D_dryfield_night_motel_lobby_801824C4[];
extern u8 D_dryfield_night_motel_lobby_801824D0[];
extern u8 D_dryfield_night_motel_lobby_801824E8[];
extern u8 D_dryfield_night_motel_lobby_801824F0[];
extern u8 D_dryfield_night_motel_lobby_801824F8[];

/// The " times" suffix appended to that panel's count rows.
extern u8 D_dryfield_night_motel_lobby_80182500[];

/// Help texts of the panel's nine rows, handed to the UI holder for the
/// selected row.
extern u8 D_dryfield_night_motel_lobby_8018250C[];
extern u8 D_dryfield_night_motel_lobby_80182538[];
extern u8 D_dryfield_night_motel_lobby_8018255C[];
extern u8 D_dryfield_night_motel_lobby_8018258C[];
extern u8 D_dryfield_night_motel_lobby_801825C0[];
extern u8 D_dryfield_night_motel_lobby_801825F4[];
extern u8 D_dryfield_night_motel_lobby_8018262C[];
extern u8 D_dryfield_night_motel_lobby_80182660[];
extern u8 D_dryfield_night_motel_lobby_80182698[];

/// The usage panel's row list.
extern UiList D_dryfield_night_motel_lobby_801826FC;

extern const char D_dryfield_night_motel_lobby_8017D638[];

/// The telephone menu's entry list.
extern UiList D_dryfield_night_motel_lobby_80182784;

/// The "Play Data" panel's row list.
extern UiList D_dryfield_night_motel_lobby_801826D4;

/// Labels the four menu-entry handlers `func_dryfield_night_motel_lobby_8017F18C`
/// to `func_dryfield_night_motel_lobby_8017F400` draw: "Save", "Play Data",
/// "Weapon Data" and "PE Data".
extern u8 D_dryfield_night_motel_lobby_80182488[];
extern u8 D_dryfield_night_motel_lobby_80182490[];
extern u8 D_dryfield_night_motel_lobby_8018249C[];
extern u8 D_dryfield_night_motel_lobby_801824A8[];

/// UI descriptors the "Play Data" entry and the two usage entries open.
extern UiObjectDesc D_dryfield_night_motel_lobby_8018273C;
extern UiObjectDesc D_dryfield_night_motel_lobby_80182758;

/// The room's task descriptor table: entry 0 is the cap (cutscene) task,
/// entry 1 the sound-event task it runs alongside.
extern TaskDesc D_dryfield_night_motel_lobby_801827A8;

/// The room's message table, which `func_dryfield_night_motel_lobby_8017FD9C`
/// installs on its task.
extern s32 D_dryfield_night_motel_lobby_801827CC;

extern TaskDesc D_dryfield_night_motel_lobby_801827FC;
extern TaskDesc D_dryfield_night_motel_lobby_801828D4;

extern Task* D_dryfield_night_motel_lobby_801844CC;
extern Task* D_dryfield_night_motel_lobby_801844D0;
extern s32   D_dryfield_night_motel_lobby_801844D4;

/// The "%" suffix appended to the percentages the play-data panels print.
extern u8 D_dryfield_night_motel_lobby_80182508[];

/// UI descriptor the "Play Data" panel and the usage panel spawn when they
/// first open.
extern UiObjectDesc D_dryfield_night_motel_lobby_80182720;

/// The cap script `func_dryfield_night_motel_lobby_8017FB7C` fills in and
/// hands to the cap task it spawns.
extern RoomCutsceneRec D_dryfield_night_motel_lobby_801844E0;

void func_dryfield_night_motel_lobby_8017E310(UiList* list, UiObject* obj);
void func_dryfield_night_motel_lobby_8017E60C(UiList* list, UiObject* obj);
void func_dryfield_night_motel_lobby_8017F4C8(Task* task);

/// Draws one row of the play-data statistics panel: the row label, then the
/// statistic `arg0->field_8` selects - play time, several save counters, and
/// two percentages printed with two decimals and a "%" suffix. While the row is
/// selected its help text goes to the UI holder.
void func_dryfield_night_motel_lobby_8017D748(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_dryfield_night_motel_lobby_8018250C,
                D_dryfield_night_motel_lobby_80182538,
                D_dryfield_night_motel_lobby_8018255C,
                D_dryfield_night_motel_lobby_8018258C,
                D_dryfield_night_motel_lobby_801825C0,
                D_dryfield_night_motel_lobby_801825F4,
                D_dryfield_night_motel_lobby_8018262C,
                D_dryfield_night_motel_lobby_80182660,
                D_dryfield_night_motel_lobby_80182698,
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
            func_8002E53C(&req, D_dryfield_night_motel_lobby_801824B0);
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
            func_8002E53C(&req, D_dryfield_night_motel_lobby_801824E0);
            Text_ItoaUnsigned(p, Mc_SaveData.saveCount);
            Text_Strcat(p, D_dryfield_night_motel_lobby_80182500);
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
            func_8002E53C(&req, D_dryfield_night_motel_lobby_801824B8);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CC);
            Text_Strcat(p, D_dryfield_night_motel_lobby_80182500);
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
            func_8002E53C(&req, D_dryfield_night_motel_lobby_801824BC);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CE);
            Text_Strcat(p, D_dryfield_night_motel_lobby_80182500);
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
            func_8002E53C(&req, D_dryfield_night_motel_lobby_801824C4);
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
            Text_Strcat(p, D_dryfield_night_motel_lobby_80182508);
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
            func_8002E53C(&req, D_dryfield_night_motel_lobby_801824D0);
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
            Text_Strcat(p, D_dryfield_night_motel_lobby_80182508);
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
            func_8002E53C(&req, D_dryfield_night_motel_lobby_801824E8);
            Text_ItoaUnsigned(p, Mc_SaveData.clearCount);
            Text_Strcat(p, D_dryfield_night_motel_lobby_80182500);
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
            func_8002E53C(&req, D_dryfield_night_motel_lobby_801824F0);
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
            func_8002E53C(&req, D_dryfield_night_motel_lobby_801824F8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the "Play Data" panel `func_dryfield_night_motel_lobby_8017EF98`
/// draws.
const char D_dryfield_night_motel_lobby_8017D610[] = "Play Data";

/// Text drawn in place of a row's percentage once it reaches 100 percent.
const u8 D_dryfield_night_motel_lobby_8017D61C[] = "100.0%";

/// Draws one row of the item-usage panel: the item name and icon, its share of
/// all uses as a percentage with two decimals, and a gouraud bar sized from the
/// row's bar width. While the row is selected the item is previewed, and a
/// confirm press opens the item's description.
void func_dryfield_night_motel_lobby_8017DF14(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_dryfield_night_motel_lobby_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_dryfield_night_motel_lobby_80182508);
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
void func_dryfield_night_motel_lobby_8017E310(UiList* list, UiObject* obj)
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
void func_dryfield_night_motel_lobby_8017E60C(UiList* list, UiObject* obj)
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

/// Titles of the usage panel `func_dryfield_night_motel_lobby_8017E92C` opens:
/// "Weapon Data" and "PE Data".
const char D_dryfield_night_motel_lobby_8017D624[] = "Weapon Data";
const char D_dryfield_night_motel_lobby_8017D630[] = "PE Data";

void func_dryfield_night_motel_lobby_8017E92C(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_dryfield_night_motel_lobby_801826FC;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_motel_lobby_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_motel_lobby_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_dryfield_night_motel_lobby_80182720, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_dryfield_night_motel_lobby_8017E310(list, obj);
        } else {
            func_dryfield_night_motel_lobby_8017E60C(list, obj);
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

/// The "Telephone" title `func_dryfield_night_motel_lobby_8017EAE0` draws over
/// its menu once the menu is open. The two bytes after its terminator are not
/// zero, so it stays assembly.
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby", D_dryfield_night_motel_lobby_8017D638);

void func_dryfield_night_motel_lobby_8017EAE0(Task* task)
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
    list          = &D_dryfield_night_motel_lobby_80182784;
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
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_motel_lobby_8017D638);
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

void func_dryfield_night_motel_lobby_8017EDD8(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_dryfield_night_motel_lobby_8017F4C8;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into the digit string `str` so that `decimals` digits (at most
/// the string's length) follow it, shifting them and the NUL one byte right.
/// Does nothing when `decimals` is not positive.
void func_dryfield_night_motel_lobby_8017EE34(u8* str, s32 decimals)
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
u8* func_dryfield_night_motel_lobby_8017EEA4(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_dryfield_night_motel_lobby_80182508);
    return buf;
}

void func_dryfield_night_motel_lobby_8017EF98(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_night_motel_lobby_801826D4;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_dryfield_night_motel_lobby_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_night_motel_lobby_80182720, 0, 0, 1, obj);
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
void func_dryfield_night_motel_lobby_8017F088(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

void func_dryfield_night_motel_lobby_8017F18C(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_motel_lobby_80182488, prompt->field_1C, 1, 0);
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

void func_dryfield_night_motel_lobby_8017F270(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_motel_lobby_80182490, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_motel_lobby_8018273C, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_night_motel_lobby_8017F338(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_motel_lobby_8018249C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_motel_lobby_80182758, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_dryfield_night_motel_lobby_8017F400(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_night_motel_lobby_801824A8, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_night_motel_lobby_80182758, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Exit callback `func_dryfield_night_motel_lobby_8017EDD8` installs: releases
/// `Wip_UiHolder` if this task's UI object still holds it, then frees the
/// object and kills the task.
void func_dryfield_night_motel_lobby_8017F4C8(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

void func_dryfield_night_motel_lobby_8017F504(Task* task)
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
            D_dryfield_night_motel_lobby_801844D0 = NULL;
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
            D_dryfield_night_motel_lobby_801844D0 =
                Task_SpawnFromTable(&D_dryfield_night_motel_lobby_801827A8, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_dryfield_night_motel_lobby_801844D0);
                task->state++;
            } else if (Task_PollKill(D_dryfield_night_motel_lobby_801844D0, &poll) != 0) {
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

void func_dryfield_night_motel_lobby_8017FA70(Task* task)
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

s32 func_dryfield_night_motel_lobby_8017FB00(void)
{
    return 0;
}

/// Message handler: copies the incoming message onto the outgoing one and, for
/// message 0xF with `field_5` clear, answers in `field_3` with game-flag nibble
/// 0x61 plus one. Always returns 1.
s32 func_dryfield_night_motel_lobby_8017FB08(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0xF && in->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    return 1;
}

/// Message handler for the lobby's `arg2 == 3` event: on the first visit it
/// latches the visit flag and starts the scene, otherwise it fills in the cap
/// script and spawns the cutscene task.
s32 func_dryfield_night_motel_lobby_8017FB7C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        if (GameFlag_GetNibble(0x16D) == 0) {
            GameFlag_SetNibble(0x16D, 1);
            Gp_RunCapCmd1(0xA);
            return 0;
        }
        D_dryfield_night_motel_lobby_801844E0.field_0 = 5;
        D_dryfield_night_motel_lobby_801844E0.field_1 = 1;
        if (GameFlag_GetNibble(0x7A) < 4) {
            D_dryfield_night_motel_lobby_801844E0.field_14 = 0x380;
            D_dryfield_night_motel_lobby_801844E0.field_3  = 1;
        } else {
            D_dryfield_night_motel_lobby_801844E0.field_14 = 0x3C0;
            D_dryfield_night_motel_lobby_801844E0.field_3  = 2;
        }
        D_dryfield_night_motel_lobby_801844E0.field_2  = 0;
        D_dryfield_night_motel_lobby_801844E0.field_4  = 0x53110003;
        D_dryfield_night_motel_lobby_801844E0.field_8  = 0x53110004;
        D_dryfield_night_motel_lobby_801844E0.field_10 = 0x53110005;
        D_dryfield_night_motel_lobby_801844E0.field_C  = 0x53110006;
        Task_SpawnFromTable(&D_dryfield_night_motel_lobby_801827A8, 0, 4, (s32)&D_dryfield_night_motel_lobby_801844E0);
    }
    return 0;
}

s32 func_dryfield_night_motel_lobby_8017FC6C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0x74) == 0) {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Task_SpawnFromTable(&D_dryfield_night_motel_lobby_801827FC, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(8);
        }
    }
    return 0;
}

s32 func_dryfield_night_motel_lobby_8017FCDC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        Gp_EnqueueStageSnd6(0x5311000A, 0, 0);
    }
    return 0;
}

void func_dryfield_night_motel_lobby_8017FD10(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            D_dryfield_night_motel_lobby_801844CC = Task_SpawnFromTable(&D_dryfield_night_motel_lobby_801828D4, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_dryfield_night_motel_lobby_801844CC, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

void func_dryfield_night_motel_lobby_8017FD9C(Task* task)
{
    task->msgTable = &D_dryfield_night_motel_lobby_801827CC;
    Game_SetPtrSlot(task, 7);
    D_dryfield_night_motel_lobby_801844D4 = 1;
    task->state                           = (s32)(task->state + 1);
}

void func_dryfield_night_motel_lobby_8017FDE8(Task* task)
{
    s32 temp_v0;

    temp_v0 = Gp_HasCollectedBit(0x113);
    if ((temp_v0 != 0) && (D_dryfield_night_motel_lobby_801844D4 == 0)) {
        func_800E3FAC(0xA2, 0x14);
    }
    D_dryfield_night_motel_lobby_801844D4 = temp_v0;
}

/// The three states of the task `func_dryfield_night_motel_lobby_8017FE38` runs:
/// set-up, the per-frame check, and the kill.
const TaskFuncTable3 D_dryfield_night_motel_lobby_8017D6A4 = {
    {
        func_dryfield_night_motel_lobby_8017FD9C,
        func_dryfield_night_motel_lobby_8017FDE8,
        taskKill,
    },
};

void func_dryfield_night_motel_lobby_8017FE38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_lobby_8017D6A4;
    sp.funcs[task->state](task);
}

/// Runs one frame of the lobby's examine prompt. A busy cap suspends the whole
/// scan for that frame; otherwise the cursor is hit-tested against the room's
/// hotspot table and a confirm press on a hit hotspot is handed to the keypad
/// (`func_dryfield_night_motel_lobby_80180440`) or, while the prompt is idle,
/// latches the hotspot for the next prompt state. Hotspot id 0xB is the panel
/// the keypad is read from, and a code that checks out ends the sequence in
/// state 6. A cancel press ends it in state 5.
///
/// The two paths that leave early call the cursor draw themselves and return
/// rather than jumping to a shared label: the three identical call-and-epilogue
/// tails are what GCC's cross jumping folds into one, and that fold is what
/// leaves the argument setup standing before the merged call with the branches
/// landing past it. Writing a `goto` there compiles to a different tail.
void func_dryfield_night_motel_lobby_8017FE90(Task* task)
{
    DnmlExamineWork*  work   = (DnmlExamineWork*)task->work;
    OverlayHotspot*   hs     = D_dryfield_night_motel_lobby_80182820;
    RoomActionPrompt* prompt = &D_80114D28;

    work->field_7            = 0;
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
    } else {
        prompt->targetId = 0x80;
        if (func_dryfield_night_motel_lobby_80180DE4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
            prompt->mode = 2;
            if (prompt->buttons[0].state == 2) {
                while (hs->id != -1) {
                    if (hs->hit != 0) {
                        if (work->promptBusy == 0) {
                            prompt->mode     = 0;
                            prompt->targetId = 0;
                            work->field_0    = hs->id;
                            work->promptKind = hs->promptKind;
                            task->state      = 3;
                            func_dryfield_night_motel_lobby_801802A8(task);
                            return;
                        }
                        if (work->field_6 == 0) {
                            if (hs->id == 0xB) {
                                work->field_6 = 1;
                                work->field_7 = 1;
                                SndEvt_EnqueueType6(0x53110007, 0, 0);
                            }
                            break;
                        }
                        func_dryfield_night_motel_lobby_80180440(task, hs->id);
                        if (work->field_8 != 0) {
                            Mc_SaveData.at4.loc.view = 7;
                            task->state              = 6;
                            func_dryfield_night_motel_lobby_801802A8(task);
                            return;
                        }
                        break;
                    }
                    hs++;
                }
            }
        } else {
            prompt->mode = 1;
        }
        if (prompt->buttons[1].state == 2) {
            task->state = 5;
        }
    }
    func_dryfield_night_motel_lobby_801802A8(task);
}

/// Outlines `rect` in (`r`, `g`, `b`) with four flat `LINE_F2`s, one per edge
/// of the rectangle from (`x`, `y`) to (`x + w`, `y + h`), each linked into
/// `gGpuCurrentOt[1]`.
void func_dryfield_night_motel_lobby_80180064(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}

/// The eleven states of the room's examine task, run by
/// `func_dryfield_night_motel_lobby_80180D58`.
const TaskFuncTable11 D_dryfield_night_motel_lobby_8017D6B0 = {
    {
        func_dryfield_night_motel_lobby_80180E98,
        func_dryfield_night_motel_lobby_80180FA4,
        func_dryfield_night_motel_lobby_8017FE90,
        func_dryfield_night_motel_lobby_80180FD8,
        func_dryfield_night_motel_lobby_8018103C,
        func_dryfield_night_motel_lobby_801810AC,
        func_dryfield_night_motel_lobby_80181138,
        func_dryfield_night_motel_lobby_8018119C,
        func_dryfield_night_motel_lobby_801811E0,
        func_dryfield_night_motel_lobby_80181218,
        func_dryfield_night_motel_lobby_8018122C,
    },
};

/// Zero word that closes this object's `.rodata` and puts the next object's
/// first jump table at 0x8017D6E0; nothing reads it.
const u32 D_dryfield_night_motel_lobby_8017D6DC = 0;
