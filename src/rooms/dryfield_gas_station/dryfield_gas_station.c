#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

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
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/dryfield_gas_station.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// The cutscene script `func_dryfield_gas_station_8017FD54` fills in before
/// spawning the cutscene task with it.
extern RoomCutsceneRec D_dryfield_gas_station_80184BD8;

extern void Stage_RequestFromAreaTable(s32 arg0);

extern UiObjectDesc D_800611E4;
extern UiObject*    D_80067634;
extern u8           D_80071086;
extern u8           D_80072170;
extern u16          D_80072174;
extern s8           D_80072176;
extern u16          D_80072834;
extern u16          D_80072836;
extern u8           D_80072A93;
extern s32          D_80072A94;
extern s32          D_80072A98;
extern UiObjectDesc D_8010EFA0;
extern s16          D_80114D08;
extern u8           D_80115598;

/// Saved `Mc_SaveData.at4.loc.view` (area id), restored when the cutscene ends.
extern s32 D_80115694;

/// `Mc_SaveData.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `Mc_SaveData` address with case 0.
extern s8 D_8007217B;

extern GpAreaApplyRec D_80188888[];

/// Prompt texts: "Save", "Play Data", "Weapon Data" and "PE Data".
extern u8 D_dryfield_gas_station_80181AF8[];
extern u8 D_dryfield_gas_station_80181B00[];
extern u8 D_dryfield_gas_station_80181B0C[];
extern u8 D_dryfield_gas_station_80181B18[];

/// Row labels of the "Play Data" statistics list, one per row index.
extern u8 D_dryfield_gas_station_80181B20[];
extern u8 D_dryfield_gas_station_80181B50[];
extern u8 D_dryfield_gas_station_80181B28[];
extern u8 D_dryfield_gas_station_80181B2C[];
extern u8 D_dryfield_gas_station_80181B34[];
extern u8 D_dryfield_gas_station_80181B40[];
extern u8 D_dryfield_gas_station_80181B58[];
extern u8 D_dryfield_gas_station_80181B60[];
extern u8 D_dryfield_gas_station_80181B68[];

/// The suffix appended to the statistics rows that count events.
extern u8 D_dryfield_gas_station_80181B70[];

/// The "%" suffix appended to a formatted percentage.
extern u8 D_dryfield_gas_station_80181B78[];

/// Help lines shown for the selected statistics row, one per row index.
extern u8 D_dryfield_gas_station_80181B7C[];
extern u8 D_dryfield_gas_station_80181BA8[];
extern u8 D_dryfield_gas_station_80181BCC[];
extern u8 D_dryfield_gas_station_80181BFC[];
extern u8 D_dryfield_gas_station_80181C30[];
extern u8 D_dryfield_gas_station_80181C64[];
extern u8 D_dryfield_gas_station_80181C9C[];
extern u8 D_dryfield_gas_station_80181CD0[];
extern u8 D_dryfield_gas_station_80181D08[];

/// The "Play Data" statistics list.
extern UiList D_dryfield_gas_station_80181D44;

/// The usage list shown by `func_dryfield_gas_station_8017E8DC`.
extern UiList D_dryfield_gas_station_80181D6C;

/// UI descriptor of the help-line box the "Play Data" and usage panels open
/// beside their lists.
extern UiObjectDesc D_dryfield_gas_station_80181D90;

/// UI descriptors the "Play Data" and usage prompts open.
extern UiObjectDesc D_dryfield_gas_station_80181DAC;
extern UiObjectDesc D_dryfield_gas_station_80181DC8;

/// The list shown by `func_dryfield_gas_station_8017EA90`.
extern UiList D_dryfield_gas_station_80181DF4;

extern TaskDesc D_dryfield_gas_station_80181E18;
extern TaskDesc D_dryfield_gas_station_80181E3C[];
extern s32      D_dryfield_gas_station_80181E54;
extern TaskDesc D_dryfield_gas_station_80181E7C[];
extern Task*    D_dryfield_gas_station_80184BCC;
extern Task*    D_dryfield_gas_station_80184BD0;

void func_dryfield_gas_station_8017E2C0(UiList* list, UiObject* obj);
void func_dryfield_gas_station_8017E5BC(UiList* list, UiObject* obj);
void func_dryfield_gas_station_8017F478(Task* task);

/// Draws one row of the "Play Data" statistics list: the label for row
/// `arg0->field_8` and its value (play time, save count, battles won and
/// escaped, the two percentages, clear count, maximum EXP and BP).
/// While the row is the selected one it also posts that row's help line.
void func_dryfield_gas_station_8017D6F8(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_dryfield_gas_station_80181B7C,
                D_dryfield_gas_station_80181BA8,
                D_dryfield_gas_station_80181BCC,
                D_dryfield_gas_station_80181BFC,
                D_dryfield_gas_station_80181C30,
                D_dryfield_gas_station_80181C64,
                D_dryfield_gas_station_80181C9C,
                D_dryfield_gas_station_80181CD0,
                D_dryfield_gas_station_80181D08,
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
            func_8002E53C(&req, D_dryfield_gas_station_80181B20);
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
            func_8002E53C(&req, D_dryfield_gas_station_80181B50);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_dryfield_gas_station_80181B70);
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
            func_8002E53C(&req, D_dryfield_gas_station_80181B28);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_dryfield_gas_station_80181B70);
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
            func_8002E53C(&req, D_dryfield_gas_station_80181B2C);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_dryfield_gas_station_80181B70);
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
            func_8002E53C(&req, D_dryfield_gas_station_80181B34);
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
            Text_Strcat(p, D_dryfield_gas_station_80181B78);
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
            func_8002E53C(&req, D_dryfield_gas_station_80181B40);
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
            Text_Strcat(p, D_dryfield_gas_station_80181B78);
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
            func_8002E53C(&req, D_dryfield_gas_station_80181B58);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_dryfield_gas_station_80181B70);
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
            func_8002E53C(&req, D_dryfield_gas_station_80181B60);
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
            func_8002E53C(&req, D_dryfield_gas_station_80181B68);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the "Play Data" panel.
const char D_dryfield_gas_station_8017D610[] = "Play Data";

/// Drawn in place of the percentage for a row holding every recorded use.
const u8 D_dryfield_gas_station_8017D61C[] = "100.0%";

/// Draws one row of the weapon or PE usage list: the entry's name and icon, its
/// share of all uses as a percentage with two decimals ("100.0%" for a full
/// share), and a gouraud bar of the row's width. The selected row previews the
/// entry, and pad bit 0x10 on it opens the entry's detail panel.
void func_dryfield_gas_station_8017DEC4(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_dryfield_gas_station_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_dryfield_gas_station_80181B78);
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

/// Fills the weapon-usage list's work block (`RoomItemUsage`) from the save's
/// per-weapon use counters (`Mc_SaveData.weaponUseCounts`, ids 0x80-0x9F).
///
/// Every id whose name is non-empty (a leading 0 or 0xA marks an unused row)
/// and whose counter is non-zero is marked seen and appended to `itemIds`,
/// while the counters are summed. The ids are then insertion-sorted by use
/// count, most-used first. Finally each row gets `percents` - its share of all
/// recorded uses in hundredths of a percent, rounded - and `barWidths`, its
/// counter as a 12-bit fraction of the top row's. Both are scaled down by
/// halving until the top counter fits in 17 bits, so the multiply and the
/// shift cannot overflow.
void func_dryfield_gas_station_8017E2C0(UiList* list, UiObject* obj)
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

/// Fills the PE-usage list's work block (`RoomPeUsage`) the same way from the
/// save's per-slot Parasite Energy use counters.
///
/// Each of the twelve Parasite Energy slots owns three consecutive ids starting
/// at 0xF, one per level, so slot `i` at level `Mc_SaveData.attachLevels[i]`
/// lists as `i * 3 + 0xF + level - 1` (a slot never levelled keeps the base
/// id). Every slot with a non-zero counter in `Mc_SaveData.attachUseCounts` -
/// read twelve entries wide, past the seven the struct names - is appended and
/// its counter summed. The ids are then insertion-sorted by use count,
/// most-used first, and each row gets `percents` and `barWidths` exactly as the
/// weapon list does.
void func_dryfield_gas_station_8017E5BC(UiList* list, UiObject* obj)
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
const char D_dryfield_gas_station_8017D624[] = "Weapon Data";
const char D_dryfield_gas_station_8017D630[] = "PE Data";

/// Task body of the weapon (`spawnArg1 == 0`) or PE usage panel: on its first
/// tick it allocates the list's work block and fills it, then updates the list
/// each tick, closes on cancel and tears down children that finished.
void func_dryfield_gas_station_8017E8DC(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_dryfield_gas_station_80181D6C;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_dryfield_gas_station_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_dryfield_gas_station_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_dryfield_gas_station_80181D90, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_dryfield_gas_station_8017E2C0(list, obj);
        } else {
            func_dryfield_gas_station_8017E5BC(list, obj);
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

/// Title of the "Telephone" menu. A byte after its terminator is not zero,
/// so the string stays assembly.
extern const char D_dryfield_gas_station_8017D638[];
INCLUDE_RODATA("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station", D_dryfield_gas_station_8017D638);

/// Task body of the "Telephone" menu: shows its list once the save has a
/// clear or the demo scene flag set (otherwise it spawns the fallback panel),
/// and turns a child's selection into an item prompt.
void func_dryfield_gas_station_8017EA90(Task* task)
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
    list          = &D_dryfield_gas_station_80181DF4;
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
        Ui_DrawText((UiPanel*)obj, D_dryfield_gas_station_8017D638);
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
void func_dryfield_gas_station_8017ED88(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_dryfield_gas_station_8017F478;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void func_dryfield_gas_station_8017EDE4(u8* str, s32 decimals)
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
u8* func_dryfield_gas_station_8017EE54(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_dryfield_gas_station_80181B78);
    return buf;
}

/// Task body of the "Play Data" panel: lays out its statistics list on the
/// first tick, then updates it each tick and closes on cancel.
void func_dryfield_gas_station_8017EF48(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_dryfield_gas_station_80181D44;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_dryfield_gas_station_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_dryfield_gas_station_80181D90, 0, 0, 1, obj);
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
void func_dryfield_gas_station_8017F038(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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
void func_dryfield_gas_station_8017F13C(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_gas_station_80181AF8, prompt->field_1C, 1, 0);
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
void func_dryfield_gas_station_8017F220(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_gas_station_80181B00, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_gas_station_80181DAC, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Prompt row "Weapon Data": on confirm opens the usage panel for weapons.
void func_dryfield_gas_station_8017F2E8(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_gas_station_80181B0C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_gas_station_80181DC8, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Prompt row "PE Data": on confirm opens the usage panel for Parasite
/// Energy.
void func_dryfield_gas_station_8017F3B0(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_dryfield_gas_station_80181B18, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_dryfield_gas_station_80181DC8, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Exit callback of the help-line box task: releases `Wip_UiHolder` if the
/// task owns it, then frees the task's UI object and kills it.
void func_dryfield_gas_station_8017F478(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// Task body of the room's cutscene: `spawnArg2` is its
/// `RoomCutsceneRec`. It hides the HUD and the weapons, forces the
/// script's area, loads and plays the cap file while a child task from
/// `D_dryfield_gas_station_80181E18` runs (confirm or cancel skips it), then
/// runs the cap command for the story's progress, restores the area and the
/// HUD, and kills itself; states 20-23 are the branch that runs follow-up
/// commands 0x20 / 0x21 by the cap's event key.
void func_dryfield_gas_station_8017F4B4(Task* task)
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
            D_dryfield_gas_station_80184BD0 = NULL;
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
            D_dryfield_gas_station_80184BD0 =
                Task_SpawnFromTable(&D_dryfield_gas_station_80181E18, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_dryfield_gas_station_80184BD0);
                task->state++;
            } else if (Task_PollKill(D_dryfield_gas_station_80184BD0, &poll) != 0) {
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

/// Answers the room message `in`, copying it to `out` first. For message 2 it
/// reports in `out->field_3` how far nibble 0x61 has advanced (3 once nibble
/// 0x7A reaches 4). Message 3 returns 2 when the session sits at stage 3,
/// place 1 with `Gp_StateF0` agreeing, and 0 while nibble 0x3B is clear;
/// message 2 returns 0 while nibble 0x45 reads 1. The cap commands and nibble
/// write that go with those answers run only when `in->field_5` is clear.
/// Every other case returns 1.
s32 func_dryfield_gas_station_8017FA20(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
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
/// the gas station, then enqueues it as a type-6 sound event. Event key 0x83
/// only plays if a cap script is still reporting an event key. Keys with no
/// sound are ignored. Always returns 0.
s32 func_dryfield_gas_station_8017FB94(s32 arg0, s32 arg1, s32 arg2)
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

/// Task body that enqueues the type-6 sound event held in `spawnArg2` on its
/// first tick and again at tick 0x50, and kills itself at tick 0x78; `state`
/// counts the ticks.
void func_dryfield_gas_station_8017FCBC(Task* task)
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

/// Always returns 0.
s32 func_dryfield_gas_station_8017FD4C(void)
{
    return 0;
}

/// Cutscene trigger for the gas station. On request 1, if the `0x16B` flag is
/// clear it raises it and asks the cap system to run command 0xB; otherwise it
/// fills in the room's cap script (area 8, this request as the slot and file)
/// and spawns `D_dryfield_gas_station_80181E18`. Returns 1 when the request is
/// not 1, otherwise the spawned task.
s32 func_dryfield_gas_station_8017FD54(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x16B) == 0) {
            GameFlag_SetNibble(0x16B, 1);
            Gp_RunCapCmd1(0xB);
            return 0;
        }
        if (Mc_SaveData.at4.loc.warp == arg2) {
            Mc_SaveData.at4.loc.warp = 2;
        }
        D_dryfield_gas_station_80184BD8.field_0  = 8;
        D_dryfield_gas_station_80184BD8.field_1  = arg2;
        D_dryfield_gas_station_80184BD8.field_3  = arg2;
        D_dryfield_gas_station_80184BD8.field_2  = 0;
        D_dryfield_gas_station_80184BD8.field_4  = 0x52010005;
        D_dryfield_gas_station_80184BD8.field_8  = 0x52010007;
        D_dryfield_gas_station_80184BD8.field_10 = 0x52010008;
        D_dryfield_gas_station_80184BD8.field_C  = 0x52010010;
        return (s32)Task_SpawnFromTable(&D_dryfield_gas_station_80181E18, 0, 2, (s32)&D_dryfield_gas_station_80184BD8);
    }
    return 1;
}

/// Spawns the room's event task and stores it in `D_dryfield_gas_station_80184BCC`,
/// waits for it to be killed, then kills this task.
void func_dryfield_gas_station_8017FE20(Task* arg0)
{
    s32 state = arg0->state;
    s32 out;

    switch (state) {
        case 0:
            D_dryfield_gas_station_80184BCC = Task_SpawnFromTable(D_dryfield_gas_station_80181E7C, 0, 0, 0);
            arg0->state++;
            break;
        case 1:
            if (Task_PollKill(D_dryfield_gas_station_80184BCC, &out) != 0) {
                arg0->state++;
            }
            break;
        case 2:
            taskKill(arg0);
            break;
    }
}

/// State 0 of the gas-station cutscene task. On the first visit
/// (`D_80072170 == 1`) it spawns the room's event task and clears the three
/// progression flags; otherwise it just asks the stage for area 1. Either way
/// it advances to state 1 and raises the `D_80115598` flag.
void func_dryfield_gas_station_8017FEDC(Task* arg0)
{
    arg0->msgTable = &D_dryfield_gas_station_80181E54;
    Game_SetPtrSlot(arg0, 7);
    if (D_80072170 == 1) {
        Task_SpawnFromTable(D_dryfield_gas_station_80181E3C, 0, 0, 0);
        GameFlag_SetNibble(0x7A, 2);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 0);
    } else {
        Stage_RequestFromAreaTable(1);
    }
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// State 1 of the room's main task: does nothing.
void func_dryfield_gas_station_8017FF84(Task* task)
{
}

/// The three states of the room's main task, run by
/// `func_dryfield_gas_station_8017FF8C`: set-up, the per-frame handler and the
/// kill.
const TaskFuncTable3 D_dryfield_gas_station_8017D6A4 = {
    { func_dryfield_gas_station_8017FEDC, func_dryfield_gas_station_8017FF84, taskKill },
};

/// Dispatches the task through the room's three-state table, copied onto the
/// stack first.
void func_dryfield_gas_station_8017FF8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_gas_station_8017D6A4;
    sp.funcs[task->state](task);
}
