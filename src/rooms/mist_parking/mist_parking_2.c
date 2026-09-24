#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/4CC.h"
#include "rooms/room_common.h"
#include "rooms/mist_parking.h"

extern UiObjectDesc D_8010EFA0;
extern u16          D_80072174;
extern s8           D_80072176;
extern u16          D_80072834;
extern u16          D_80072836;
extern u8           D_80072A93;
extern s32          D_80072A94;
extern s32          D_80072A98;
extern u8           D_mist_parking_801866C0[];
extern u8           D_mist_parking_801866F0[];
extern u8           D_mist_parking_801866C8[];
extern u8           D_mist_parking_801866CC[];
extern u8           D_mist_parking_801866D4[];
extern u8           D_mist_parking_801866E0[];
extern u8           D_mist_parking_801866F8[];
extern u8           D_mist_parking_80186700[];
extern u8           D_mist_parking_80186708[];
extern u8           D_mist_parking_80186710[];
extern u8           D_mist_parking_8018671C[];
extern u8           D_mist_parking_80186748[];
extern u8           D_mist_parking_8018676C[];
extern u8           D_mist_parking_8018679C[];
extern u8           D_mist_parking_801867D0[];
extern u8           D_mist_parking_80186804[];
extern u8           D_mist_parking_8018683C[];
extern u8           D_mist_parking_80186870[];
extern u8           D_mist_parking_801868A8[];

void func_mist_parking_801800D0(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_mist_parking_8018671C,
                D_mist_parking_80186748,
                D_mist_parking_8018676C,
                D_mist_parking_8018679C,
                D_mist_parking_801867D0,
                D_mist_parking_80186804,
                D_mist_parking_8018683C,
                D_mist_parking_80186870,
                D_mist_parking_801868A8,
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
            func_8002E53C(&req, D_mist_parking_801866C0);
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
            func_8002E53C(&req, D_mist_parking_801866F0);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_mist_parking_80186710);
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
            func_8002E53C(&req, D_mist_parking_801866C8);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_mist_parking_80186710);
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
            func_8002E53C(&req, D_mist_parking_801866CC);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_mist_parking_80186710);
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
            func_8002E53C(&req, D_mist_parking_801866D4);
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
            Text_Strcat(p, D_mist_parking_80186718);
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
            func_8002E53C(&req, D_mist_parking_801866E0);
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
            Text_Strcat(p, D_mist_parking_80186718);
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
            func_8002E53C(&req, D_mist_parking_801866F8);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_mist_parking_80186710);
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
            func_8002E53C(&req, D_mist_parking_80186700);
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
            func_8002E53C(&req, D_mist_parking_80186708);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", D_mist_parking_8017D748);

/// One row of the "Play Data" item-usage list: the item's name, its share of
/// all recorded uses as `NN.NN%` (or a flat `100.0%` once it is the only item
/// used), and a gauge whose width is the row's `barWidths` fraction of the
/// panel. Confirming the row opens the item's detail panel.

void func_mist_parking_8018089C(DialogPrompt* prompt, UiObject* obj)
{
    u8             buf[0x20];
    u8*            p;
    u8*            q;
    RoomItemUsage* work;
    POLY_G4*       prim;
    s32            itemId;
    s32            pct;
    s32            scale;
    s32            remaining;
    s32            i;
    s32            len;
    s32            n;
    s32            right;
    s32            lo;
    s32            barY;
    s32            ry;
    s32            color;
    s32            barW;
    s32            barX;
    s32            x0;
    s32            x1;
    s32            y0;
    s32            status;
    s32            one;
    s32            px;
    s32            py;
    TextDrawReq    req;
    TextDrawReq*   r;

    p = buf;
    /* The request's address is live across Gp_GetItemText, so the last field is
       written through it while the rest stay sp-relative. */
    r      = &req;
    work   = (RoomItemUsage*)obj->owner->work;
    itemId = work->itemIds[prompt->field_8];
    pct    = work->percents[prompt->field_8];
    px     = prompt->field_18;
    py     = prompt->field_1A;
    color  = prompt->field_1C;

    if (obj->mode != 5) {
        req.x          = obj->baseX + 0x11 + px;
        ry             = obj->baseY - 6;
        req.y          = ry + py;
        req.otIndex    = (s16)obj->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        func_8002E53C(r, (u8*)Gp_GetItemText(itemId, 0, 0));
        func_800CE5D0(obj, px, py, itemId);
    }

    if (pct >= 0x2710) {
        Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, "100.0%", prompt->field_1C, 3, 2);
    } else {
        scale     = 1;
        remaining = 2;
        do {
            scale *= 10;
            remaining--;
        } while (remaining > 0);

        if (pct < scale) {
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
        Text_Strcat(p, D_mist_parking_80186718);
        Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, buf, prompt->field_1C, 3, 2);
    }

    lo    = (s16)obj->field_1C + 0x80;
    right = (s16)obj->field_1E - 0x4A;
    barY  = (s16)prompt->field_1A - 0xC;
    barW  = right - lo;
    barW  = (barW * work->barWidths[prompt->field_8]) >> 12;
    barW += 2;
    barX  = right - barW;
    if (barW >= 2) {
        prim     = (POLY_G4*)gGpuPrimCursor;
        x0       = obj->baseX + barX + 1;
        prim->x2 = x0;
        prim->x0 = x0;

        gGpuPrimCursor   = prim + 1;
        y0               = obj->baseY;
        y0               = y0 + barY;
        y0              += 1;
        *(u32*)&prim->r3 = 0x10000;
        *(u32*)&prim->r1 = 0x10000;
        setlen(prim, 8);
        *(u32*)&prim->r0 = 0x100B0;
        setcode(prim, 0x38);
        *(u32*)&prim->r2 = 0x100B0;

        x1 = (u16)prim->x0 + barW;
        x1--;
        prim->y1 = y0;
        prim->y0 = y0;
        y0      += 8;
        prim->y3 = y0;
        prim->y2 = y0;
        prim->x3 = x1;
        prim->x1 = x1;
        addPrim(gGpuCurrentOt + (s16)obj->drawOrder + 1, prim);
    }

    one = 1;
    func_80046B34(obj, barX, (s16)prompt->field_1A - 0xC, barW, 9, 0, one);

    status = obj->status;
    if (((status >> 16) == one) || (status == one)) {
        if (prompt->field_10 == prompt->field_8) {
            Gp_SetPreviewItem(itemId, 0);
            Gp_SetHolderItemText(itemId);
        }
    }

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, 0x10) != 0) {
        SndEvt_EnqueueType6(3, 0, 0);
        Ui_SpawnFromDesc(&D_8010EFA0, itemId, 1, 1, obj);
        obj->status = 0;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", D_mist_parking_8017D75C);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", D_mist_parking_8017D768);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", D_mist_parking_8017D770);
