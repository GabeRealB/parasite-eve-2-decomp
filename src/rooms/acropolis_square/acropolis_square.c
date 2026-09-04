#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "main/display.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"

#include <psyq/libgpu.h>

extern UiObjectDesc D_8010EFA0;

/// The "%" suffix appended to a row's percentage; every carrying room has its
/// own copy, named at that room's address by the family's symbol maps.
extern u8 Room_Util38Percent[];

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_8017D714);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_8017D8C8);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_square/acropolis_square", D_acropolis_square_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_8017F24C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_8017F41C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_8017F46C);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_square/acropolis_square", D_acropolis_square_8017D620);

/// Draws one row of a "Play Data" usage panel: the item name, its share of the
/// recorded uses as a percentage, and a gauge whose length is the row's
/// `barWidths` fraction of the panel's inner width. The three parallel arrays
/// come from the `RoomItemUsage` block the panel's task parked in
/// `Task::idMap`; `RoomPeUsage` has the same layout, so the same row handler
/// serves the Parasite Energy panel.
///
/// A share of 10000 (a full 100%) prints as the literal "100.0%"; anything else
/// is printed with `decimals = 2` and then shifted one place right so the last
/// two digits become the fraction. Highlighting the row previews its item, and
/// pressing the item-detail button on the selected row opens `D_8010EFA0`.
void func_acropolis_square_8017FC38(DialogPrompt* arg0, UiObject* arg1)
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
    work  = (RoomItemUsage*)arg1->owner->idMap;
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, (u8*)"100.0%", arg0->field_1C, 3, 2);
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
        Text_Strcat(p, Room_Util38Percent);
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
        prim             = (POLY_G4*)Gpu_PrimCursor;
        tx               = arg1->baseX + barX + 1;
        prim->x2         = tx;
        prim->x0         = tx;
        ty               = arg1->baseY;
        Gpu_PrimCursor   = (DR_TPAGE*)(prim + 1);
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
        addPrim(Gpu_CurrentOt + (s16)arg1->drawOrder + 1, prim);
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
INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_80180034);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_80180330);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_80180650);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_80180804);

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square", func_acropolis_square_80180AFC);
