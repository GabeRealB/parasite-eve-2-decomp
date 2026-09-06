#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include "main/display.h"
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

extern UiObject*      D_80067634;
extern UiObjectDesc   D_8010EFA0;
extern GpItemScan     D_80072724;
extern RoomShopStock  D_8010E138[];
extern u8             D_80071072;
extern u8             RoomsShared8017f938Bp[];
extern u8             D_mist_parking_80186450[];
extern u8             RoomsShared8017fdb8Msg[];
extern u8             D_mist_parking_801864BC[];
extern u8             D_mist_parking_801864C4[];
extern u8             D_mist_parking_801864D0[];
extern u8             D_mist_parking_801864D8[];
extern u8             RoomsShared8017f31cMsg0[];
extern u8             RoomsShared8017f31cMsg1[];
extern u8             RoomsShared8017f31cMsg2[];
extern u8             RoomsShared8017f764Amount[];
extern u8             RoomsShared8017f938Times[];
extern u8             D_mist_parking_80186718[];
extern UiListItemFunc D_mist_parking_80186538[];
extern UiList         D_mist_parking_80186540;
extern UiObjectDesc   D_mist_parking_801865AC;
extern UiObjectDesc   D_mist_parking_80186654;
extern UiObjectDesc   D_mist_parking_801865C8;
extern UiList         RoomsShared8017fe74List;
extern UiObjectDesc   RoomsShared8017ff9cDesc;
extern UiObjectDesc   D_mist_parking_80186600;
extern UiObjectDesc   D_mist_parking_80186670;
extern char           Gp_StrEmpty[];
extern s32            RoomsShared8017f49cQty;
extern GpItemMap*     RoomsShared8017f49cMap;

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801800D0);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", RoomsShared8017ef20Title);

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
    work   = (RoomItemUsage*)obj->owner->idMap;
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
        prim     = (POLY_G4*)Gpu_PrimCursor;
        x0       = obj->baseX + barX + 1;
        prim->x2 = x0;
        prim->x0 = x0;

        Gpu_PrimCursor   = (DR_TPAGE*)(prim + 1);
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
        addPrim(Gpu_CurrentOt + (s16)obj->drawOrder + 1, prim);
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

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", RoomsShared8017e8b4WeaponTitle);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", RoomsShared8017e8b4PeTitle);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", RoomsShared8017ea68Title);
