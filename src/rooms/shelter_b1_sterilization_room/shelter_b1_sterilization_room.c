#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "decomp/common.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
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
#include "rooms/rooms_shared_80181228.h"
#include "rooms/shelter_b1_sterilization_room.h"

/// A 0x18-byte message argument block passed to `Gp_DispatchMsg`; only its
/// stride is known.
typedef struct {
    u8 data[0x18];
} _ShelterB1SterilizationRoomMsg;

/// One destination: the view written to the session and the save, and the
/// index of the message block sent with it.
typedef struct {
    u8 view;
    s8 msg;
} _ShelterB1SterilizationRoomDest;

/// Row labels drawn by the statistics rows of `func_shelter_b1_sterilization_room_8017D794`.
extern u8 D_shelter_b1_sterilization_room_8018453C[];
extern u8 D_shelter_b1_sterilization_room_8018456C[];
extern u8 D_shelter_b1_sterilization_room_80184544[];
extern u8 D_shelter_b1_sterilization_room_80184548[];
extern u8 D_shelter_b1_sterilization_room_80184550[];
extern u8 D_shelter_b1_sterilization_room_8018455C[];
extern u8 D_shelter_b1_sterilization_room_80184574[];
extern u8 D_shelter_b1_sterilization_room_8018457C[];
extern u8 D_shelter_b1_sterilization_room_80184584[];

/// Unit suffix appended to the counted rows.
extern u8 D_shelter_b1_sterilization_room_8018458C[];

/// Help lines shown for each statistics row while it is selected.
extern u8 D_shelter_b1_sterilization_room_80184598[];
extern u8 D_shelter_b1_sterilization_room_801845C4[];
extern u8 D_shelter_b1_sterilization_room_801845E8[];
extern u8 D_shelter_b1_sterilization_room_80184618[];
extern u8 D_shelter_b1_sterilization_room_8018464C[];
extern u8 D_shelter_b1_sterilization_room_80184680[];
extern u8 D_shelter_b1_sterilization_room_801846B8[];
extern u8 D_shelter_b1_sterilization_room_801846EC[];
extern u8 D_shelter_b1_sterilization_room_80184724[];

/// Prompt lines drawn by the dialog handlers.
extern u8 D_shelter_b1_sterilization_room_80184514[];
extern u8 D_shelter_b1_sterilization_room_8018451C[];
extern u8 D_shelter_b1_sterilization_room_80184528[];
extern u8 D_shelter_b1_sterilization_room_80184534[];

/// List state of the "Play Data" menu `func_shelter_b1_sterilization_room_8017EFE4` runs.
extern UiList D_shelter_b1_sterilization_room_80184760;

/// List state of the weapon- and PE-usage panels.
extern UiList D_shelter_b1_sterilization_room_80184788;

/// UI descriptors the dialog handlers open on confirm.
extern UiObjectDesc D_shelter_b1_sterilization_room_801847C8;
extern UiObjectDesc D_shelter_b1_sterilization_room_801847E4;

/// List state of the menu `func_shelter_b1_sterilization_room_8017EB2C` runs.
extern UiList D_shelter_b1_sterilization_room_80184810;

extern s32                             D_shelter_b1_sterilization_room_80184E40;
extern TaskDesc                        D_shelter_b1_sterilization_room_80184E70;
extern s32                             D_shelter_b1_sterilization_room_80184E7C;
extern s16                             D_shelter_b1_sterilization_room_80184E80;
extern s16                             D_shelter_b1_sterilization_room_80184E82;
extern GpGridParams                    D_shelter_b1_sterilization_room_80184F28;
extern s32                             D_shelter_b1_sterilization_room_80188590;
extern s32                             D_shelter_b1_sterilization_room_80188624;
extern _ShelterB1SterilizationRoomMsg  D_shelter_b1_sterilization_room_80188668[];
extern _ShelterB1SterilizationRoomDest D_shelter_b1_sterilization_room_80188728[];
extern GpU16Pair                       D_shelter_b1_sterilization_room_80188738;
extern s32                             D_shelter_b1_sterilization_room_8018873C;

/// Area records `func_shelter_b1_sterilization_room_8017F550` applies when it
/// advances game flag nibble 0 from 2 to 3 in one particular view.
extern GpAreaApplyRec D_shelter_b1_sterilization_room_80188888;

extern s32            D_shelter_b1_sterilization_room_80188AB4;
extern s32            D_shelter_b1_sterilization_room_80188ED4;
extern s32            D_shelter_b1_sterilization_room_80188FDC;
extern GpGridParams   D_shelter_b1_sterilization_room_80189E44;
extern GpObj4A        D_shelter_b1_sterilization_room_8018BF30[];
extern GpAreaApplyRec D_shelter_b1_sterilization_room_8018C334;

/// The task `func_shelter_b1_sterilization_room_8017F550` spawns from entry 1
/// of its table and waits on, or NULL while none runs.
extern Task* D_shelter_b1_sterilization_room_8018C33C;

extern RoomCutsceneRec D_shelter_b1_sterilization_room_8018C344;

extern UiObjectDesc D_800611E4;
extern UiObject*    D_80067634;
extern u8           D_80071086;
extern u16          D_80072174;
extern s8           D_80072176;
extern u16          D_80072834;
extern u16          D_80072836;
extern u8           D_80072A93;
extern s32          D_80072A94;
extern s32          D_80072A98;
extern s16          D_80073BA0;
extern UiObjectDesc D_8010EFA0;
extern s16          D_80114D08;
extern u32          D_80115694;
extern s32          D_80135AC0;
extern s32          D_80135D78;
extern s32          D_80136258;

extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);
extern s32  func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

void func_shelter_b1_sterilization_room_8017E35C(UiList* list, UiObject* obj);
void func_shelter_b1_sterilization_room_8017E658(UiList* list, UiObject* obj);
void func_shelter_b1_sterilization_room_8017FABC(Task* task);
void func_shelter_b1_sterilization_room_80180340(s32 arg0);
void func_shelter_b1_sterilization_room_80180464(Task* task);
void func_shelter_b1_sterilization_room_8018049C(void);
void func_shelter_b1_sterilization_room_80180570(GpCoord* coord, s16* arg1);
void func_shelter_b1_sterilization_room_80180828(Task* task);
void func_shelter_b1_sterilization_room_80181244(Task* task);
void func_shelter_b1_sterilization_room_801812A0(Task* task);

void func_shelter_b1_sterilization_room_8017D794(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_shelter_b1_sterilization_room_80184598,
                D_shelter_b1_sterilization_room_801845C4,
                D_shelter_b1_sterilization_room_801845E8,
                D_shelter_b1_sterilization_room_80184618,
                D_shelter_b1_sterilization_room_8018464C,
                D_shelter_b1_sterilization_room_80184680,
                D_shelter_b1_sterilization_room_801846B8,
                D_shelter_b1_sterilization_room_801846EC,
                D_shelter_b1_sterilization_room_80184724,
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
            func_8002E53C(&req, D_shelter_b1_sterilization_room_8018453C);
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
            func_8002E53C(&req, D_shelter_b1_sterilization_room_8018456C);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_shelter_b1_sterilization_room_8018458C);
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
            func_8002E53C(&req, D_shelter_b1_sterilization_room_80184544);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_shelter_b1_sterilization_room_8018458C);
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
            func_8002E53C(&req, D_shelter_b1_sterilization_room_80184548);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_shelter_b1_sterilization_room_8018458C);
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
            func_8002E53C(&req, D_shelter_b1_sterilization_room_80184550);
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
            Text_Strcat(p, D_shelter_b1_sterilization_room_80184594);
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
            func_8002E53C(&req, D_shelter_b1_sterilization_room_8018455C);
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
            Text_Strcat(p, D_shelter_b1_sterilization_room_80184594);
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
            func_8002E53C(&req, D_shelter_b1_sterilization_room_80184574);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_shelter_b1_sterilization_room_8018458C);
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
            func_8002E53C(&req, D_shelter_b1_sterilization_room_8018457C);
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
            func_8002E53C(&req, D_shelter_b1_sterilization_room_80184584);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the "Play Data" panel.
const char D_shelter_b1_sterilization_room_8017D610[] = "Play Data";

/// Drawn in place of the percentage for a row holding every recorded use.
const u8 D_shelter_b1_sterilization_room_8017D61C[] = "100.0%";

void func_shelter_b1_sterilization_room_8017DF60(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_shelter_b1_sterilization_room_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_shelter_b1_sterilization_room_80184594);
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

/// Fills the weapon-usage panel's `RoomItemUsage` block from the save's
/// per-weapon use counters (ids 0x80-0x9F). Each id with a non-empty name and
/// a non-zero counter is marked seen and listed, most-used first; each row
/// then gets its share of all uses in hundredths of a percent and a bar width
/// as a 12-bit fraction of the top row, both scaled down until the top counter
/// fits in 17 bits.
void func_shelter_b1_sterilization_room_8017E35C(UiList* list, UiObject* obj)
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

/// Fills the PE-usage panel's `RoomPeUsage` block the same way from the
/// save's per-slot Parasite Energy counters. Slot `i` owns three ids from
/// `i * 3 + 0xF`, one per level, and is listed under the id of its current
/// level.
void func_shelter_b1_sterilization_room_8017E658(UiList* list, UiObject* obj)
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
const char D_shelter_b1_sterilization_room_8017D624[] = "Weapon Data";
const char D_shelter_b1_sterilization_room_8017D630[] = "PE Data";

void func_shelter_b1_sterilization_room_8017E978(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_shelter_b1_sterilization_room_80184788;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_shelter_b1_sterilization_room_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_shelter_b1_sterilization_room_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847AC, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_shelter_b1_sterilization_room_8017E35C(list, obj);
        } else {
            func_shelter_b1_sterilization_room_8017E658(list, obj);
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

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room", D_shelter_b1_sterilization_room_8017D638);

void func_shelter_b1_sterilization_room_8017EB2C(Task* task)
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
    list          = &D_shelter_b1_sterilization_room_80184810;
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
        Ui_DrawText((UiPanel*)obj, D_shelter_b1_sterilization_room_8017D638);
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

void func_shelter_b1_sterilization_room_8017EE24(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_shelter_b1_sterilization_room_8017F514;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into the digit string `str` so that `decimals` digits (at
/// most the string's length) follow it. Does nothing when `decimals <= 0`.
void func_shelter_b1_sterilization_room_8017EE80(u8* str, s32 decimals)
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

/// Formats `value` into `buf` as a percentage with `decimals` digits after the
/// point, zero-padding a small value so that a digit precedes the point, then
/// appends "%" and returns `buf`.
u8* func_shelter_b1_sterilization_room_8017EEF0(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_shelter_b1_sterilization_room_80184594);
    return buf;
}

void func_shelter_b1_sterilization_room_8017EFE4(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b1_sterilization_room_80184760;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_shelter_b1_sterilization_room_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847AC, 0, 0, 1, obj);
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
/// (`arg1`, `arg2`) from the panel origin and `arg3` by `arg4` in size, with
/// `arg5` on the left vertices and `arg6` on the right. Nothing is drawn for a
/// zero `arg5` or a width below 2.
void func_shelter_b1_sterilization_room_8017F0D4(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

void func_shelter_b1_sterilization_room_8017F1D8(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_sterilization_room_80184514, prompt->field_1C, 1, 0);
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

void func_shelter_b1_sterilization_room_8017F2BC(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_sterilization_room_8018451C, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847C8, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_shelter_b1_sterilization_room_8017F384(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_sterilization_room_80184528, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847E4, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_shelter_b1_sterilization_room_8017F44C(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_sterilization_room_80184534, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_sterilization_room_801847E4, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_shelter_b1_sterilization_room_8017F514(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// Runs a cutscene described by the `RoomCutsceneRec` in `spawnArg2`:
/// hides the HUD and holds the player, optionally switches the view and loads
/// a cap file, plays the cap slot while waiting on a spawned task that the
/// confirm or cancel button can cut short, then runs the follow-up cap
/// commands, restores the view and releases the player.
void func_shelter_b1_sterilization_room_8017F550(Task* task)
{
    RoomCutsceneRec* rec;
    s32              killOut;
    s32              flag;
    s32              cmd;
    s32              fadeA;
    s32              fadeB;

    rec = (RoomCutsceneRec*)task->spawnArg2;
    switch (task->state) {
        case 0:
            D_shelter_b1_sterilization_room_8018C33C = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (rec->field_0 > 0) {
                D_80115694               = Mc_SaveData.at4.loc.view;
                Mc_SaveData.at4.loc.view = rec->field_0;
            } else {
                D_80115694 = -rec->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (rec->field_4 != 0) {
                SndEvt_EnqueueType6(rec->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (rec->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(rec->field_3);
                fadeB = 0;
                fadeA = rec->field_14;
                if (fadeA == 0) {
                    fadeA = 0x3C0;
                } else {
                    fadeB = rec->field_16;
                }
                func_800E6D4C(fadeA, fadeB);
            }
            if (rec->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_shelter_b1_sterilization_room_8018C33C = Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80184E1C, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_shelter_b1_sterilization_room_8018C33C);
                task->state++;
            } else if (Task_PollKill(D_shelter_b1_sterilization_room_8018C33C, &killOut) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (rec->field_2 == 0) {
                SndEvt_EnqueueType6(rec->field_C, 0, 0);
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
            if (rec->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(rec->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((GP_LOC_WORD(Mc_SaveData.at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 1, 0, 0)) {
                        Gp_ApplyAreaRecs(&D_shelter_b1_sterilization_room_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
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
            SndEvt_EnqueueType6(rec->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (rec->field_3 != 0) {
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

/// The three states of the room's main task, run by
/// `func_shelter_b1_sterilization_room_80180518`: set-up, the per-frame
/// handler and the kill.
const TaskFuncTable3 D_shelter_b1_sterilization_room_8017D6A4 = {
    {
        func_shelter_b1_sterilization_room_8017FABC,
        func_shelter_b1_sterilization_room_80180464,
        taskKill,
    },
};

void func_shelter_b1_sterilization_room_8017FABC(Task* task)
{
    Task* target;

    task->msgTable = &D_shelter_b1_sterilization_room_80184E40;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 5 && GameFlag_GetNibble(0xEA) == 0) {
        GameFlag_SetNibble(0xF4, 3);
        Gp_ApplyAreaRecs(&D_shelter_b1_sterilization_room_8018C334);
        if (gameGetPtrSlot(0xA) != NULL) {
            GameFlag_SetNibble(0x116, 1);
            GameFlag_SetNibble(0xEA, 2);
            GameFlag_SetNibble(0x4B, 8);
            func_800E8634((s32)&D_80135D78, 0, (s32)&D_80136258);
            Gp_SetAreaObjId((GpAreaKey*)&gGameSession->at4.loc, 6, 1);
        } else {
            GameFlag_SetNibble(0x116, 2);
            GameFlag_SetNibble(0xEA, 1);
            func_800E8634((s32)&D_80135AC0, 0, (s32)&D_80136258);
        }
    }
    func_shelter_b1_sterilization_room_80180340(0);
    if (gGameSession->at4.loc.place == 5) {
        target = (Task*)Gp_LookupSlot4(0);
        if (target != NULL) {
            Gp_DispatchMsg(target, 0x7DB, (s32)&D_shelter_b1_sterilization_room_80184E7C, 0);
        }
    }
    if (GameFlag_GetNibble(0xEA) != 1) {
        D_shelter_b1_sterilization_room_8018BF30[0].field_4A &= 0xBF;
    }
    if (gGameSession->at4.loc.place == 1) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 6, 0, 0);
    }
    task->state++;
}

s32 func_shelter_b1_sterilization_room_8017FC78(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    s32 cmd;
    s32 mask;
    s32 flags;

    switch (msg->field_2) {
        case 1:
            if (GameFlag_GetNibble(0x76) == 0) {
                if (GameFlag_GetNibble(0x84) != 0) {
                    func_800E8634((s32)&D_shelter_b1_sterilization_room_8018873C, 0,
                                  (s32)&D_shelter_b1_sterilization_room_80188AB4);
                    GameFlag_SetNibble(0x76, 1);
                }
            }
            break;
        case 2:
            if (GameFlag_GetNibble(0x76) == 1) {
                if (GameFlag_GetNibble(0x77) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 1, 0, 0);
                } else {
                    Gp_RunCapCmd1(0x17);
                }
            } else {
                Gp_RunCapCmd1(0x16);
            }
            break;
        case 5:
        case 8:
            switch (msg->field_2) {
                case 5:
                    cmd  = 5;
                    mask = 2;
                    break;
                case 8:
                    cmd  = 3;
                    mask = 8;
                    break;
                default:
                    cmd  = 0;
                    mask = 0xFF;
                    break;
            }
            if (GameFlag_GetNibble(0x76) == 0 || GameFlag_GetNibble(0x77) == 1) {
                flags = GameFlag_GetNibble(0xF2);
                if (cmd != 0 && !(flags & mask)) {
                    Gp_RunCapCmd1(cmd);
                    GameFlag_SetNibble(0xF2, flags | mask);
                }
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->field_2 - 3, 0);
            } else {
                Gp_RunCapCmd1(0xA);
            }
            break;
        case 3:
        case 6:
        case 7:
        case 10:
            if (GameFlag_GetNibble(0x76) == 0 || GameFlag_GetNibble(0x77) == 1) {
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 4, msg->field_2 - 3, 0);
            } else {
                Gp_RunCapCmd1(0xA);
            }
            break;
        case 9:
            if (GameFlag_GetNibble(0x76) == 0 || GameFlag_GetNibble(0x77) == 1) {
                if (GameFlag_GetNibble(0x149) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 1, 0);
                } else {
                    if (GameFlag_GetNibble(0x150) == 0) {
                        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 2, 0);
                        GameFlag_SetNibble(0x150, 1);
                    }
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->field_2 - 3, 0);
                }
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0x77) == 0) {
                if (GameFlag_GetNibble(0x151) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 3, 0);
                    GameFlag_SetNibble(0x151, 1);
                }
            } else if (GameFlag_GetNibble(0x151) < 2) {
                if (GameFlag_GetNibble(0x14A) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 4, 0);
                } else {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 5, 0);
                }
                GameFlag_SetNibble(0x151, 2);
            }
            Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->field_2 - 3, 0);
            break;
    }
    return 0;
}

s32 func_shelter_b1_sterilization_room_8017FF80(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x11) {
        if (GameFlag_GetNibble(0x161) == 0) {
            GameFlag_SetNibble(0x161, 1);
            Gp_SpawnIfCapIdle(0x18, 1);
            return;
        }
        D_shelter_b1_sterilization_room_8018C344.field_0  = 0x13;
        D_shelter_b1_sterilization_room_8018C344.field_1  = 1;
        D_shelter_b1_sterilization_room_8018C344.field_3  = 2;
        D_shelter_b1_sterilization_room_8018C344.field_2  = 0;
        D_shelter_b1_sterilization_room_8018C344.field_4  = 0x5410000C;
        D_shelter_b1_sterilization_room_8018C344.field_8  = 0x5410000F;
        D_shelter_b1_sterilization_room_8018C344.field_10 = 0x5410000D;
        D_shelter_b1_sterilization_room_8018C344.field_C  = 0x5410000E;
        Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80184E1C, 0, 6, (s32)&D_shelter_b1_sterilization_room_8018C344);
    }
    if (arg2 == 0x15) {
        Gp_RunCapCmd1(arg2);
    }
    if (arg2 == 4) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 7, 0, 0);
    }
    if (arg2 == 0x13) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 7, 1, 0);
    }
    if (arg2 == 0xE) {
        if (GameFlag_GetNibble(0x76) == 1 && GameFlag_GetNibble(0x77) == 0) {
            if (GameFlag_GetNibble(0x14F) == 0) {
                Gp_MsgPlayerWeapon(0);
                func_800E8634((s32)&D_shelter_b1_sterilization_room_80188ED4, 0, (s32)&D_shelter_b1_sterilization_room_80188FDC);
                GameFlag_SetNibble(0x14F, 1);
            } else {
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 0xB, 0);
                GameFlag_SetNibble(0x14F, 2);
            }
        } else {
            Gp_RunCapCmd1(arg2);
        }
    }
    if (arg2 == 0xC || arg2 == 0xD) {
        if (gGameSession->at4.loc.room == 3) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80184E70, 0, arg2, 0);
        }
    }
    return 0;
}

void func_shelter_b1_sterilization_room_80180188(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 0, 0);
            task->state++;
            break;
        case 1:
            task->state++;
            break;
        case 2:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 3:
            Gp_RunCapCmd(task->spawnArg1, 0);
            task->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 5:
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 1, 0);
            task->state++;
            break;
        case 6:
            task->state++;
            break;
        case 7:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 8:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

/// Plays the sound event in `spawnArg2` in states 0 and 0x50, then kills the
/// task at state 0x78.
void func_shelter_b1_sterilization_room_801802B0(Task* task)
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

void func_shelter_b1_sterilization_room_80180340(s32 arg0)
{
    Task* slot   = (Task*)Gp_LookupSlot4(0);
    Task* task   = slot;
    s32   isNull = (slot == NULL);

    if (isNull) {
        task = gameGetPtrSlot(3);
    }
    if (slot != NULL) {
        if (gGameSession->at4.loc.place == 5 && GameFlag_GetNibble(0xEA) == 1) {
            D_shelter_b1_sterilization_room_80184E82 = 0;
        } else {
            D_shelter_b1_sterilization_room_80184E82 = 0x2710;
        }
    } else {
        D_shelter_b1_sterilization_room_80184E82 = 0x2710;
    }
    func_shelter_b1_sterilization_room_80180570(task->extra.tmd->coords, &D_shelter_b1_sterilization_room_80184E80);
}

s32 func_shelter_b1_sterilization_room_801803E4(void)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one,
/// passes both to `func_80179A04` and returns 1.
s32 func_shelter_b1_sterilization_room_801803EC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

s32 func_shelter_b1_sterilization_room_80180430(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x54100016, 0, 0);
    }
    return 0;
}

void func_shelter_b1_sterilization_room_80180464(Task* task)
{
    if (gGameSession->at4.loc.place == 5) {
        func_shelter_b1_sterilization_room_8018049C();
    }
}

void func_shelter_b1_sterilization_room_8018049C(void)
{
    s32 view;

    view = gGameSession->at4.loc.view;
    if ((GameFlag_GetNibble(0xEA) == 1) && (gGameSession->eventState == 0)) {
        if (view == 2 || view == 3) {
            Gp_MsgSlot4Chain(0, 1);
        } else {
            Gp_MsgSlot4Chain(0, 0);
        }
    }
}

void func_shelter_b1_sterilization_room_80180518(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_sterilization_room_8017D6A4;
    sp.funcs[task->state](task);
}

void func_shelter_b1_sterilization_room_80180570(GpCoord* coord, s16* arg1)
{
    MATRIX        m;
    long          flag;
    s32           i;
    SVECTOR*      d;
    SVECTOR*      s;
    GpGridParams* dst = &D_shelter_b1_sterilization_room_80189E44;
    GpGridParams* src = &D_shelter_b1_sterilization_room_80184F28;

    i = 0;
    do {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
        i++;
    } while (i < 4);

    i = 0;
    do {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
        i++;
    } while (i < 8);

    m = coord->coord;

    if (arg1 != NULL) {
        m.t[0] += arg1[0];
        m.t[1] += arg1[1];
        m.t[2] += arg1[2];
    }

    d = dst->field_4;
    s = src->field_4;
    i = 0;
    do {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
        i++;
    } while (i < 4);

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->field_8;
    s = src->field_8;
    for (i = 0; i < 8; i++) {
        RotTransSV(s++, d++, &flag);
    }
}

/// Queues two VRAM copies of the frame buffer being drawn, a 0xC0-wide strip to
/// (0x340, 0) and the next 0x80 columns to (0x180, 0x100), bracketed by STP
/// commands that set the mask bit off before them and back on after. The
/// source row follows the live draw buffer. The task then advances a state.
void func_shelter_b1_sterilization_room_80180828(Task* task)
{
    RECT     rect;
    DR_STP*  stp;
    DR_MOVE* mv;
    s16      x;
    s16      y;

    if (gDisplayState.drawBuffer == 0) {
        x = 0;
        y = 0;
    } else {
        x = 0;
        y = 0x110;
    }

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(gGpuCurrentOt + 8, stp);

    mv             = (DR_MOVE*)gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x;
    rect.y         = y;
    rect.w         = 0xC0;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x340, 0);
    addPrim(gGpuCurrentOt + 8, mv);

    mv             = (DR_MOVE*)gGpuPrimCursor;
    gGpuPrimCursor = mv + 1;
    rect.x         = x + 0xC0;
    rect.y         = y;
    rect.w         = 0x80;
    rect.h         = 0xF0;
    SetDrawMove(mv, &rect, 0x180, 0x100);
    addPrim(gGpuCurrentOt + 8, mv);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(gGpuCurrentOt + 8, stp);

    task->state++;
}

/// Draws the room's two backdrop halves as opaque `SPRT`s in OT slot 8, tinted
/// by `shade`. The source rows, both the sprites' `v` and the tpage row, follow
/// the display buffer being drawn.
void func_shelter_b1_sterilization_room_80180A2C(s32 shade)
{
    SPRT* p;
    s16   tpageY;
    u8    u;
    u8    v;

    if (gDisplayState.drawBuffer == 0) {
        tpageY = 0;
        u      = 0;
        v      = 0;
    } else {
        tpageY = 0x100;
        u      = 0;
        v      = 0x10;
    }

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = u;
    p->v0   = v;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0, tpageY);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->x0   = 0x20;
    p->u0   = u;
    p->v0   = v;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0xC0, tpageY);
}

/// Redraw the room's two backdrop halves as semi-transparent `SPRT`s in OT
/// slot 8, tinting both with `shade`, then append each half's tpage.
void func_shelter_b1_sterilization_room_80180BF0(s32 shade)
{
    SPRT* p;

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0x340, 0);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = 0x20;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    func_shelter_b1_sterilization_room_80181308(0x180, 0x100);
}

void func_shelter_b1_sterilization_room_80180D74(Task* task)
{
    s32 c;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            SndEvt_EnqueueType6(0x5410000A, 0, 0);
            task->state++;
            break;
        case 1:
            if (++task->killCountdown >= 30) {
                task->state++;
            }
            c = (task->killCountdown * 0xFF / 30) & 0xFF;
            Fade_DrawOverlay(c, c, c, 2);
            break;
        case 2:
            Mc_SaveData.at4.loc.view   = D_shelter_b1_sterilization_room_80188728[task->spawnArg1].view;
            gGameSession->at4.loc.view = D_shelter_b1_sterilization_room_80188728[task->spawnArg1].view;
            gGameSession->viewDirty    = 1;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9,
                           (s32)&D_shelter_b1_sterilization_room_80188668[D_shelter_b1_sterilization_room_80188728[task->spawnArg1].msg],
                           0);
            SndEvt_EnqueueType6(0x5410000B, 0, 0);
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            task->state++;
            break;
        case 3:
            if (--task->killCountdown <= 0) {
                task->state++;
            }
            c = (task->killCountdown * 0xFF / 30) & 0xFF;
            Fade_DrawOverlay(c, c, c, 2);
            break;
        default:
            Gp_MsgPlayerWeapon(1);
            taskKill(task);
            break;
    }
}

void func_shelter_b1_sterilization_room_80180F74(Task* task)
{
    Task*       player;
    GpCoord*    coord;
    s32         pan;
    GpStateC08* st;

    switch (task->state) {
        case 0:
            gGameSession->restartMode = 2;
            task->state++;
            return;
        case 1:
            if (GameFlag_GetNibble(0x77) == 0) {
                if (gGameSession->eventState == 0 && Gp_StateF0.field_4 == 0) {
                    player = gameGetPtrSlot(3);
                    task->killCountdown++;
                    if (task->killCountdown == 0x78) {
                        Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_shelter_b1_sterilization_room_80188738, 0), 0);
                    } else if (task->killCountdown >= 0x79) {
                        if (D_80073BA0 > 0) {
                            coord = player->extra.tmd->coords;
                            Gp_DispatchMsg(player, 0x3F7, (s32)&D_shelter_b1_sterilization_room_80188590, 0);
                            Gp_PlayerWeaponId(&D_shelter_b1_sterilization_room_80188624);
                            Gp_DispatchMsg(player, 0x3E8, (s32)&D_shelter_b1_sterilization_room_80188624, 0);
                            pan = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(0x54100011, pan, (s8)gpGetObjDepth(coord));
                            task->killCountdown = 0;
                        }
                        st           = &Gp_StateC08;
                        st->field_6 |= 1;
                    } else if (task->killCountdown == 0x49) {
                        Gp_MsgPlayerWeapon(1);
                    }
                    task->spawnArg1 = 0;
                    return;
                }
                if (task->spawnArg1 == 0) {
                    SndEvt_EnqueueType7(0x54100011, 1);
                    task->spawnArg1 = 1;
                }
                return;
            }
            SndEvt_EnqueueType7(0x54100011, 1);
        default:
            taskKill(task);
            break;
    }
}

void func_shelter_b1_sterilization_room_8018118C(s32 arg0)
{
    if (!(((s32)D_shelter_b1_sterilization_room_8018C340 >> arg0) & 1)) {
        D_shelter_b1_sterilization_room_8018C340 |= 1 << arg0;
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, arg0, 0, 0);
    }
}

/// The four states of the backdrop task run by
/// `func_shelter_b1_sterilization_room_801811E0`: copy the frame buffer into
/// the backdrop, wait for the view, fade the copy out and kill.
const TaskFuncTable4 D_shelter_b1_sterilization_room_8017D700 = {
    {
        func_shelter_b1_sterilization_room_80180828,
        func_shelter_b1_sterilization_room_80181244,
        func_shelter_b1_sterilization_room_801812A0,
        taskKill,
    },
};

void func_shelter_b1_sterilization_room_801811E0(Task* task)
{
    TaskFuncTable4 states;

    states = D_shelter_b1_sterilization_room_8017D700;
    states.funcs[task->state](task);
}

void func_shelter_b1_sterilization_room_80181244(Task* task)
{
    func_shelter_b1_sterilization_room_80180BF0(0x80);
    func_shelter_b1_sterilization_room_80180A2C(0);
    if (gGameSession->viewReady != 0) {
        task->killCountdown = 0x80;
        task->state++;
    }
}

/// Steps `killCountdown` down by 8 each frame, advancing the task once it
/// reaches zero, and redraws the backdrop with the semi-transparent copy at
/// that level and the opaque copy at the rest.
void func_shelter_b1_sterilization_room_801812A0(Task* task)
{
    u16 fade;

    fade                = (u16)task->killCountdown - 8;
    task->killCountdown = fade;
    if ((s16)fade <= 0) {
        task->killCountdown = 0;
        task->state++;
    }
    func_shelter_b1_sterilization_room_80180BF0(task->killCountdown);
    func_shelter_b1_sterilization_room_80180A2C(0x80 - task->killCountdown);
}

void func_shelter_b1_sterilization_room_80181308(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(2, 1, tpage & 0x3C0, y));
    addPrim(gGpuCurrentOt + 8, p);
}
