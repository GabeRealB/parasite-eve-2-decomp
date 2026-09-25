#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "decomp/common.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
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
#include "main/ui.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_80181228.h"
#include "rooms/shelter_r47.h"

/// Sparse view of the adjacent records at 0x80187960 and 0x80187C0C.
/// The room's 0x13EF message handler toggles bit 0x40 at offset 0x4A.
typedef struct ShelterR47Object {
    u8 pad_0[0x4A];
    u8 field_4A;
    u8 pad_4B[0x261];
} ShelterR47Object;
STATIC_ASSERT_SIZEOF(ShelterR47Object, 0x2AC);

/// One textured piece of a sprite drawn by `func_shelter_r47_80180F38`: its
/// CLUT position, its offset from the sprite's origin, and its texture window.
/// A piece whose `clutX` is 0xFFFF ends the list.
typedef struct {
    u16 clutX;
    u16 clutY;
    s16 x;
    s16 y;
    u8  u;
    u8  v;
    u8  w;
    u8  h;
} ShelterR47SpritePart;

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
extern UiObjectDesc D_8010EFA0;
extern s8           D_80114C12;
extern s16          D_80114D08;
extern u32          D_80115694;
extern s32          D_801350BC;
extern s32          D_801359D4;
extern s32          D_8014152C;
extern s32          D_80141C1C;
extern s32          D_80141D9C;
extern s32          D_80142A74;
extern s32          D_80142C24;
extern s32          D_801432FC;
extern s32          D_80143494;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Row labels of the "Play Data" statistics panel.
extern u8 D_shelter_r47_80186C10[];
extern u8 D_shelter_r47_80186C18[];
extern u8 D_shelter_r47_80186C1C[];
extern u8 D_shelter_r47_80186C24[];
extern u8 D_shelter_r47_80186C30[];
extern u8 D_shelter_r47_80186C40[];
extern u8 D_shelter_r47_80186C48[];
extern u8 D_shelter_r47_80186C50[];
extern u8 D_shelter_r47_80186C58[];
/// Unit suffix appended to the panel's counts.
extern u8 D_shelter_r47_80186C60[];
/// The "%" suffix the room's percentage formatters append.
extern u8 D_shelter_r47_80186C68[];
/// Help lines shown for the panel's rows, by row.
extern u8 D_shelter_r47_80186C6C[];
extern u8 D_shelter_r47_80186C98[];
extern u8 D_shelter_r47_80186CBC[];
extern u8 D_shelter_r47_80186CEC[];
extern u8 D_shelter_r47_80186D20[];
extern u8 D_shelter_r47_80186D54[];
extern u8 D_shelter_r47_80186D8C[];
extern u8 D_shelter_r47_80186DC0[];
extern u8 D_shelter_r47_80186DF8[];

extern u8     D_shelter_r47_80186BE8[];
extern u8     D_shelter_r47_80186BF0[];
extern u8     D_shelter_r47_80186BFC[];
extern u8     D_shelter_r47_80186C08[];
extern UiList D_shelter_r47_80186E34;
extern UiList D_shelter_r47_80186E5C;
/// UI object descriptor that the "Play Data" page tasks spawn as a child on
/// their first frame.
extern UiObjectDesc D_shelter_r47_80186E80;
extern UiObjectDesc D_shelter_r47_80186E9C;
extern UiObjectDesc D_shelter_r47_80186EB8;
extern UiList       D_shelter_r47_80186EE4;

/// Task descriptor table: the room spawns entry 0 with a cutscene record as its
/// argument, and `func_shelter_r47_8017F628` spawns entry 1 while it waits for
/// the player to skip.
extern TaskDesc D_shelter_r47_80186F08;

/// Message table the room's controller task answers with.
extern GpMsgEntry D_shelter_r47_80186F2C[];

/// Ally animation descriptor handed to `Gp_AllyAnimId`, then forwarded as the
/// payload of the 0x3E8 message.
extern s32 D_shelter_r47_80186F5C;

extern TaskDesc D_shelter_r47_80186F70;
extern TaskDesc D_shelter_r47_80186F94;

/// Hotspot table hit-tested by `func_shelter_r47_80182B9C`.
extern OverlayHotspot D_shelter_r47_80186FB4[];

extern TaskDesc D_shelter_r47_80187020;

/// Piece lists of the sprites `func_shelter_r47_80180F38` draws, by sprite id.
extern ShelterR47SpritePart* D_shelter_r47_8018729C[];

extern TaskDesc         D_shelter_r47_801872F0;
extern TaskDesc         D_shelter_r47_80187618;
extern ShelterR47Object D_shelter_r47_80187960[];
extern ShelterR47Object D_shelter_r47_80187C0C;

extern GpAreaApplyRec D_shelter_r47_80188888;
extern Task*          D_shelter_r47_8018A68C;

/// Task spawned by the room's cap script; polled and cleared by
/// `func_shelter_r47_80180714`.
extern Task* D_shelter_r47_8018A690;

extern RoomCutsceneRec D_shelter_r47_8018A698;

void func_shelter_r47_8017E434(UiList* list, UiObject* obj);
void func_shelter_r47_8017E730(UiList* list, UiObject* obj);
void func_shelter_r47_8017F5EC(Task* task);
void func_shelter_r47_8017FB94(Task* task);
void func_shelter_r47_8017FCC0(void);

void func_shelter_r47_8017D86C(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_shelter_r47_80186C6C,
                D_shelter_r47_80186C98,
                D_shelter_r47_80186CBC,
                D_shelter_r47_80186CEC,
                D_shelter_r47_80186D20,
                D_shelter_r47_80186D54,
                D_shelter_r47_80186D8C,
                D_shelter_r47_80186DC0,
                D_shelter_r47_80186DF8,
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
            func_8002E53C(&req, D_shelter_r47_80186C10);
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
            func_8002E53C(&req, D_shelter_r47_80186C40);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_shelter_r47_80186C60);
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
            func_8002E53C(&req, D_shelter_r47_80186C18);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_shelter_r47_80186C60);
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
            func_8002E53C(&req, D_shelter_r47_80186C1C);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_shelter_r47_80186C60);
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
            func_8002E53C(&req, D_shelter_r47_80186C24);
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
            Text_Strcat(p, D_shelter_r47_80186C68);
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
            func_8002E53C(&req, D_shelter_r47_80186C30);
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
            Text_Strcat(p, D_shelter_r47_80186C68);
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
            func_8002E53C(&req, D_shelter_r47_80186C48);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_shelter_r47_80186C60);
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
            func_8002E53C(&req, D_shelter_r47_80186C50);
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
            func_8002E53C(&req, D_shelter_r47_80186C58);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the "Play Data" panel.
const char D_shelter_r47_8017D610[] = "Play Data";

/// Drawn in place of the percentage for a row holding every recorded use.
const u8 D_shelter_r47_8017D61C[] = "100.0%";

void func_shelter_r47_8017E038(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_shelter_r47_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_shelter_r47_80186C68);
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
void func_shelter_r47_8017E434(UiList* list, UiObject* obj)
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

/// Parasite Energy counterpart of `func_shelter_r47_8017E434`: fills the "Play
/// Data" PE-usage panel's `RoomPeUsage` block from the save's per-slot use
/// counters.
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
void func_shelter_r47_8017E730(UiList* list, UiObject* obj)
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
const char D_shelter_r47_8017D624[] = "Weapon Data";
const char D_shelter_r47_8017D630[] = "PE Data";

void func_shelter_r47_8017EA50(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_shelter_r47_80186E5C;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_shelter_r47_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_shelter_r47_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_shelter_r47_80186E80, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_shelter_r47_8017E434(list, obj);
        } else {
            func_shelter_r47_8017E730(list, obj);
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

/// Title of the "Telephone" panel. The word holding its terminator carries two
/// more non-zero bytes, so it stays assembly.
INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47", D_shelter_r47_8017D638);

extern const char D_shelter_r47_8017D638[];

void func_shelter_r47_8017EC04(Task* task)
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
    list          = &D_shelter_r47_80186EE4;
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
        Ui_DrawText((UiPanel*)obj, D_shelter_r47_8017D638);
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

void func_shelter_r47_8017EEFC(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_shelter_r47_8017F5EC;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void func_shelter_r47_8017EF58(u8* str, s32 decimals)
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
u8* func_shelter_r47_8017EFC8(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_shelter_r47_80186C68);
    return buf;
}

void func_shelter_r47_8017F0BC(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_r47_80186E34;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_shelter_r47_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_r47_80186E80, 0, 0, 1, obj);
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
void func_shelter_r47_8017F1AC(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

void func_shelter_r47_8017F2B0(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_r47_80186BE8, prompt->field_1C, 1, 0);
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

void func_shelter_r47_8017F394(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_r47_80186BF0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_r47_80186E9C, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_shelter_r47_8017F45C(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_r47_80186BFC, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_r47_80186EB8, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_shelter_r47_8017F524(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_r47_80186C08, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_r47_80186EB8, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
void func_shelter_r47_8017F5EC(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

void func_shelter_r47_8017F628(Task* task)
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
            D_shelter_r47_8018A68C = NULL;
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
            D_shelter_r47_8018A68C = Task_SpawnFromTable(&D_shelter_r47_80186F08, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_shelter_r47_8018A68C);
                task->state++;
            } else if (Task_PollKill(D_shelter_r47_8018A68C, &killOut) != 0) {
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
                    if ((*(u32*)&Mc_SaveData.at4.loc.view & 0xFFFF0000) == 0x1010000) {
                        Gp_ApplyAreaRecs(&D_shelter_r47_80188888);
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

/// The three states of the room's main task, run by `func_shelter_r47_801807B4`:
/// set-up, the per-frame handler and the kill.
const TaskFuncTable3 D_shelter_r47_8017D6A4 = {
    {
        func_shelter_r47_8017FB94,
        func_shelter_r47_8017FCC0,
        taskKill,
    },
};

void func_shelter_r47_8017FB94(Task* task)
{
    Task* player;

    task->msgTable = D_shelter_r47_80186F2C;
    Game_SetPtrSlot(task, 7);
    player = gameGetPtrSlot(0xA);
    if (player != NULL && GameFlag_GetNibble(0x80) == 0 && GameFlag_GetNibble(0xD1) == 1) {
        Gp_DispatchMsg(player, 0x3F3, 0, 0);
        Gp_AllyAnimId(&D_shelter_r47_80186F5C);
        Gp_DispatchMsg(player, 0x3E8, (s32)&D_shelter_r47_80186F5C, 0);
    }
    D_shelter_r47_8018A690 = NULL;
    func_shelter_r47_80183210();
    Task_SpawnFromTable(&D_shelter_r47_80186F70, 1, 0, 0);
    if (GameFlag_GetNibble(0x83) == 1 || GameFlag_GetNibble(0x80) == 1) {
        D_shelter_r47_80187960[0].field_4A &= 0xBF;
    } else {
        D_shelter_r47_80187C0C.field_4A &= 0xBF;
    }
    task->state++;
}

void func_shelter_r47_8017FCC0(void)
{
    u8 place = gGameSession->at4.loc.place;

    if (place != 1 || D_80114C12 == place) {
        return;
    }
    switch (GameFlag_GetNibble(0x82)) {
        case 1:
            if (gGameSession->eventState == 0) {
                D_shelter_r47_8018A690 = Task_SpawnFromTable(&D_shelter_r47_80187020, 0, 1, 0);
                Gp_MsgPlayer3F3(0);
                Gp_MsgPlayerWeapon(0);
                Gp_MsgSlot4Chain(0, 0);
                GameFlag_SetNibble(0x82, 2);
            }
            break;
        case 2:
            if (gGameSession->cutsceneHold == 0) {
                func_800E8634((s32)&D_80141D9C, 0, (s32)&D_80142A74);
                GameFlag_SetNibble(0x82, 3);
            }
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                D_shelter_r47_8018A690 = Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 2, 0);
                Gp_MsgPlayer3F3(0);
                Gp_MsgPlayerWeapon(0);
                Gp_MsgSlot4Chain(0, 0);
                GameFlag_SetNibble(0x82, 4);
            }
            break;
        case 4:
            if (gGameSession->cutsceneHold == 0) {
                func_800E8634((s32)&D_80142C24, 0, (s32)&D_801432FC);
                GameFlag_SetNibble(0x82, 5);
            }
            break;
    }
}

s32 func_shelter_r47_8017FE84(s32 arg0, s32 arg1, RoomEventMsg* arg2)
{
    Task*             spawned_p;
    Task*             spawned_p6;
    Task*             spawned_a;
    Task*             spawned_a0;
    Task*             spawned_a1;
    s32               flag_a;
    s32               flag_b;
    s32               kind;
    u8                field9;
    ShelterR47Object* p;
    ShelterR47Object* q;

    field9 = gGameSession->at4.loc.place;
    if (field9 == 1) {
        switch (arg2->field_2) {
            case 2:
                if ((GameFlag_GetNibble(0x83) == 1) && (GameFlag_GetNibble(0x81) == 0)) {
                    SndEvt_EnqueueType6(0x542F0001, -0xA, 0x40);
                    GameFlag_SetNibble(0x81, 1);
                }
                break;
            case 3:
                if ((GameFlag_GetNibble(0x83) == 1) && (GameFlag_GetNibble(0x82) == 0)) {
                    func_800E3FAC(0xA2, 0x2B);
                    GameFlag_SetNibble(3, 0);
                    GameFlag_SetNibble(0x155, 5);
                    func_800E8634((s32)&D_8014152C, 0, (s32)&D_80141C1C);
                    GameFlag_SetNibble(0x82, 1);
                    if (GameFlag_GetNibble(0x165) == 0) {
                        flag_a = 0x165;
                        flag_b = 1;
                        goto set_and_toggle;
                    }
                    goto toggle_only;
                }
                break;
            case 4:
                if ((GameFlag_GetNibble(0x83) == 1) && (GameFlag_GetNibble(0x82) >= 4)) {
                    func_800E8614((s32)&D_80143494, 0);
                }
                break;
            case 5:
                spawned_p              = Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_p;
                if (spawned_p != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgSlot4Chain(0, 0);
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                break;
            case 6:
                spawned_p6             = Task_SpawnFromTable(&D_shelter_r47_80187020, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_p6;
                if (spawned_p6 != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgSlot4Chain(0, 0);
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                break;
        }
    } else if (field9 == 2) {
        kind = arg2->field_2;
        if (kind < 6) {
            if (kind < 4) {
                if ((kind == 1) && (GameFlag_GetNibble(0x83) == 0) && (GameFlag_GetNibble(0x80) == 0)) {
                    if (gameGetPtrSlot(0xA) != 0) {
                        func_800E8634((s32)&D_801350BC, 0, (s32)&D_801359D4);
                    }
                    func_800E3FAC(0xA2, 0x2A);
                    GameFlag_SetNibble(0x80, 1);
                    GameFlag_SetNibble(0x4C, 8);
                    GameFlag_SetNibble(0xD1, 2);
                    Gp_FillAllyHp();
                    GameFlag_SetNibble(3, 0);
                    flag_a = 0x155;
                    flag_b = 4;
                set_and_toggle:
                    GameFlag_SetNibble(flag_a, flag_b);
                toggle_only:
                    p            = D_shelter_r47_80187960;
                    q            = p + 1;
                    p->field_4A &= 0xBF;
                    q->field_4A |= 0x40;
                }
            } else {
                spawned_a0             = Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_a0;
                if (spawned_a0 != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    if (gameGetPtrSlot(0xA) != 0) {
                        Gp_MsgAlly3F3(0);
                        Gp_MsgAllyWeapon(0);
                    }
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                goto done;
            }
        } else {
            if (kind == 6) {
                goto spawn_six;
            }
        }
    } else {
        switch (arg2->field_2) {
            case 4:
            case 5:
                spawned_a1             = Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_a1;
                if (spawned_a1 != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    if (gameGetPtrSlot(0xA) != 0) {
                        Gp_MsgAlly3F3(0);
                        Gp_MsgAllyWeapon(0);
                    }
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                goto done;
            case 6:
            spawn_six:
                spawned_a              = Task_SpawnFromTable(&D_shelter_r47_80187020, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_a;
                if (spawned_a != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    if (gameGetPtrSlot(0xA) != 0) {
                        Gp_MsgAlly3F3(0);
                        Gp_MsgAllyWeapon(0);
                    }
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                break;
        }
    }
done:
    return 0;
}

/// Room request handler. Request 1 plays the room's cutscene through the shared
/// runner once flag 0x13E is set (or runs CAP command 0x2A instead while the
/// 2-bit flag 0x22 reads 1); the first time, it sets that flag and spawns entry
/// 2 of the room's task table. Request 8 spawns entry 0 or 1 of the second task
/// table, depending on which of flags 0x83 and 0x80 is set.
s32 func_shelter_r47_801801DC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x13E) != 0) {
            if (Gp_GetCurBit2Flag(0x22) == arg2) {
                Gp_RunCapCmd1(0x2A);
                return 0;
            }
            D_shelter_r47_8018A698.field_0  = 0x2C;
            D_shelter_r47_8018A698.field_1  = arg2;
            D_shelter_r47_8018A698.field_3  = 3;
            D_shelter_r47_8018A698.field_2  = 0;
            D_shelter_r47_8018A698.field_4  = 0x542F000C;
            D_shelter_r47_8018A698.field_8  = 0x542F000F;
            D_shelter_r47_8018A698.field_10 = 0x542F000D;
            D_shelter_r47_8018A698.field_C  = 0x542F000E;
            Task_SpawnFromTable(&D_shelter_r47_80186F08, 0, 0xA, (s32)&D_shelter_r47_8018A698);
        } else {
            GameFlag_SetNibble(0x13E, 1);
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_r47_80186F70, 2, 1, 0);
        }
    } else if (arg2 == 8) {
        if (GameFlag_GetNibble(0x83) > 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_r47_80186F94, 0, 0, 0);
        } else if (GameFlag_GetNibble(0x80) > 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_r47_80186F94, 1, 0, 0);
        }
    }
    return 0;
}

void func_shelter_r47_80180324(Task* task)
{
    switch (task->state) {
        case 0:
            SndEvt_EnqueueType6(0x542F0010, 0, 0);
            task->spawnArg1     = gGameSession->at4.loc.view;
            task->killCountdown = gGameSession->eventState;
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState != task->killCountdown) {
                if (gGameSession->eventState != 0) {
                    SndEvt_EnqueueType7(0x542F0010, 0x3C);
                } else {
                    switch (gGameSession->at4.loc.view) {
                        case 2:
                        case 3:
                            SndEvt_EnqueueType6(0x542F0010, 0, 0);
                            break;
                        case 4:
                            SndEvt_EnqueueType6(0x542F0010, 0xC, 0x58);
                            break;
                    }
                }
            } else {
                if (gGameSession->eventState == 0 && gGameSession->viewReady != 0) {
                    switch (gGameSession->at4.loc.view) {
                        case 2:
                        case 3:
                            SndEvt_EnqueueTypeA(0x542F0010, 0, 0);
                            break;
                        case 4:
                            if (task->spawnArg1 == 3) {
                                SndEvt_EnqueueTypeA(0x542F0010, 0xC, 0x58);
                            } else {
                                SndEvt_EnqueueType6(0x542F0010, 0xC, 0x58);
                            }
                            break;
                        case 5:
                            SndEvt_EnqueueType7(0x542F0010, 0x3C);
                            break;
                    }
                }
                task->spawnArg1 = gGameSession->at4.loc.view;
            }
            task->killCountdown = gGameSession->eventState;
            break;
    }
}

void func_shelter_r47_80180540(Task* task)
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

s32 func_shelter_r47_801805D0(void)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_80179A04`. Returns 1.
s32 func_shelter_r47_801805D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

s32 func_shelter_r47_8018061C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x542F0011, 0, 0);
    }
    return 0;
}

void func_shelter_r47_80180650(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            Gp_RunCapCmd1(task->spawnArg1);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
        advance:
            task->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

void func_shelter_r47_80180714(Task* task)
{
    s32 out;

    if (Task_PollKill(D_shelter_r47_8018A690, &out) != 0) {
        Gp_MsgPlayer3F3(1);
        Gp_MsgPlayerWeapon(1);
        if (gGameSession->at4.loc.place == 1) {
            Gp_MsgSlot4Chain(0, 1);
        }
        if (gameGetPtrSlot(0xA) != NULL) {
            Gp_MsgAlly3F3(1);
            Gp_MsgAllyWeapon(1);
        }
        D_shelter_r47_8018A690 = NULL;
        taskKill(task);
    }
}

void func_shelter_r47_801807B4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r47_8017D6A4;
    sp.funcs[task->state](task);
}

void func_shelter_r47_8018080C(Task* task)
{
    s32 nibble;

    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            Gp_RunCapCmd1(8);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_ResetCap();
            Gp_MsgPlayerWeapon(1);
            nibble = GameFlag_GetNibble(0x165);
            if (nibble < 3) {
                GameFlag_SetNibble(0x165, nibble + 1);
            }
        default:
            taskKill(task);
            break;
    }
}

void func_shelter_r47_801808D4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x140, 0x100);
            Gp_RunCapCmd1(7);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_ResetCap();
            Gp_MsgPlayerWeapon(1);
        default:
            taskKill(task);
            break;
    }
}

s32 func_shelter_r47_8018097C(Task* task)
{
    ShelterR47State* work;
    POLY_G3*         tri;
    DR_MODE*         mode;
    s32              angle;
    s32              done;
    s16              i;

    work = task->work;
    done = 0;
    if (work->field_3C != 0) {
        work->field_3C -= 0x20;
        if (work->field_3C < 0) {
            work->field_3C = 0;
        }
    }
    if (work->field_3C < 0xA0) {
        if (work->field_3E != 0) {
            work->field_3E -= 0x20;
            if (work->field_3E < 0) {
                work->field_3E = 0;
            }
        }
    }
    if (work->field_3E < 0xA0) {
        if (work->field_3A != 0) {
            work->field_3A -= 0x20;
            if (work->field_3A < 0) {
                work->field_3A = 0;
            }
        }
    }
    if (work->field_3A < 0xA0) {
        if (work->field_40 != 0) {
            work->field_40 -= 0x20;
            if (work->field_40 < 0) {
                work->field_40 = 0;
                done           = 1;
            }
        }
    }
    for (i = 0; i < 0x20; i++) {
        tri            = (POLY_G3*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(tri + 1);
        setPolyG3(tri);
        setRGB0(tri, work->field_3A, work->field_3C, work->field_3E);
        setRGB1(tri, work->field_40, work->field_40, work->field_40);
        setRGB2(tri, work->field_40, work->field_40, work->field_40);
        setSemiTrans(tri, 1);
        angle   = i << 7;
        tri->x0 = 0;
        tri->y0 = 0;
        tri->x1 = rsin(angle) >> 4;
        tri->y1 = rcos(angle) >> 4;
        angle  += 0x80;
        tri->x2 = rsin(angle) >> 4;
        tri->y2 = rcos(angle) >> 4;
        addPrim(&gGpuCurrentOt[11], tri);
        mode           = (DR_MODE*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(mode + 1);
        setlen(mode, 1);
        mode->code[0] = 0xE100004A;
        addPrim(&gGpuCurrentOt[11], mode);
    }
    return done;
}

s32 func_shelter_r47_80180C48(Task* task)
{
    ShelterR47State* work;
    POLY_G3*         tri;
    DR_MODE*         mode;
    s32              angle;
    s32              done;
    s16              i;

    work = task->work;
    done = 0;
    if (work->field_3A != 0xFF) {
        work->field_3A += 0x20;
        if (work->field_3A >= 0x100) {
            work->field_3A = 0xFF;
        }
    }
    if (work->field_3A > 0x60) {
        if (work->field_3E != 0xFF) {
            work->field_3E += 0x20;
            if (work->field_3E >= 0x100) {
                work->field_3E = 0xFF;
            }
        }
    }
    if (work->field_3E > 0x60) {
        if (work->field_3C != 0xFF) {
            work->field_3C += 0x20;
            if (work->field_3C >= 0x100) {
                work->field_3C = 0xFF;
            }
        }
    }
    if (work->field_3C > 0x60) {
        if (work->field_40 != 0xFF) {
            work->field_40 += 0x20;
            if (work->field_40 >= 0x100) {
                work->field_40 = 0xFF;
                done           = 1;
            }
        }
    }
    for (i = 0; i < 0x20; i++) {
        tri            = (POLY_G3*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(tri + 1);
        setPolyG3(tri);
        setRGB0(tri, work->field_40, work->field_40, work->field_40);
        setRGB1(tri, work->field_3A, work->field_3C, work->field_3E);
        setRGB2(tri, work->field_3A, work->field_3C, work->field_3E);
        setSemiTrans(tri, 1);
        angle   = i << 7;
        tri->x0 = 0;
        tri->y0 = 0;
        tri->x1 = rsin(angle) >> 4;
        tri->y1 = rcos(angle) >> 4;
        angle  += 0x80;
        tri->x2 = rsin(angle) >> 4;
        tri->y2 = rcos(angle) >> 4;
        addPrim(&gGpuCurrentOt[11], tri);
        mode           = (DR_MODE*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(mode + 1);
        setlen(mode, 1);
        mode->code[0] = 0xE100004A;
        addPrim(&gGpuCurrentOt[11], mode);
    }
    return done;
}

/// Draws sprite `id` with its origin at (`x`, `y`), one raw-textured quad
/// per piece into OT slot 10. Sprites 1 and 2 are skipped while
/// the current view is 0x12.
void func_shelter_r47_80180F38(s16 x, s16 y, s16 id)
{
    ShelterR47SpritePart* g;
    POLY_FT4*             p;

    g = D_shelter_r47_8018729C[id];
    if (gGameSession->at4.loc.view == 0x12 && (u16)(id - 1) < 2) {
        return;
    }
    while (g->clutX != 0xFFFF) {
        p              = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(p + 1);
        setPolyFT4(p);
        setUVWH(p, g->u, g->v, g->w, g->h);
        p->tpage = 0xD;
        setShadeTex(p, 1);
        p->clut = getClut(g->clutX, g->clutY);
        setXYWH(p, x + g->x, y + g->y, g->w, g->h);
        addPrim(&gGpuCurrentOt[10], p);
        g++;
    }
}

/// Outlines `rect` in (`r`, `g`, `b`) with four flat `LINE_F2` edges linked
/// into `gGpuCurrentOt[1]`.
void func_shelter_r47_80181148(RoomRect* rect, u8 r, u8 g, u8 b)
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

void func_shelter_r47_8018138C(Task* task)
{
    ShelterR47State* work;
    OverlayHotspot*  hs;
    s32              arg1;

    work = memCalloc(0x54, false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2          = Task_SpawnFromTable(&D_shelter_r47_801872F0, 0, 1, 0);
    task->work               = work;
    work->field_4E           = Mc_SaveData.at4.loc.view;
    Mc_SaveData.at4.loc.view = 0x10;
    task->state++;
    Display_AcquireRef();

    hs = D_shelter_r47_80186FB4;
    while (hs->id != -1) {
        hs->hit = 0;
        hs++;
    }

    work->field_28             = -0xF8;
    work->field_2A             = -0x68;
    work->field_2C             = -0x98;
    work->field_2E             = 0x80;
    work->field_30             = -0x98;
    work->field_32             = 0x90;
    work->field_24             = 0xB0;
    work->field_26             = -0x68;
    work->field_0[0]           = 0xAA;
    work->field_C[0]           = -0x53;
    work->field_0[1]           = 0xBE;
    work->field_C[1]           = -0x43;
    work->field_0[2]           = 0xD2;
    work->field_C[2]           = -0x33;
    work->field_0[3]           = 0xE6;
    work->field_C[3]           = -0x23;
    work->field_0[4]           = 0xFA;
    work->field_C[4]           = -0x13;
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    func_shelter_r47_80182AA0(task);
    if (work->toggles[3] == 0) {
        work->field_52 = 0;
        work->field_46 = 0x140;
    } else {
        work->field_52 = 1;
        work->field_46 = 0;
    }
    {
        ShelterR47State* w = (ShelterR47State*)task->work;

        w->field_3A = 0xFF;
        w->field_3C = 0xFF;
        w->field_3E = 0xFF;
        w->field_40 = 0xFF;
    }
    arg1 = task->spawnArg1;
    if (arg1 == 1) {
        work->field_51 = arg1;
    }
}

/// Hotspot state of the room's first cap script: redraws the scene, then
/// hit-tests the action cursor against the room's hotspot table. A miss
/// highlights the prompt (`mode` 1); a hit with the prompt confirmed
/// (`buttons[0].state` 2) hands the raised entry's `id` / `promptKind` to the
/// work block and advances to state 4. `field_51` value 4 jumps to state 0xC,
/// and with `field_51` clear a dismissed prompt advances to state 6.
void func_shelter_r47_80181568(Task* task)
{
    ShelterR47State*  work;
    OverlayHotspot*   hs;
    RoomActionPrompt* prompt;

    hs     = D_shelter_r47_80186FB4;
    prompt = &D_80114D28;
    work   = (ShelterR47State*)task->work;
    func_shelter_r47_80181914(task, 0);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    if (work->field_51 == 4) {
        task->state = 0xC;
        return;
    }
    prompt->targetId = 0x80;
    if (func_shelter_r47_80182B9C(task, hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if ((prompt->buttons[0].state == 2) && (hs->id != -1)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode       = 0;
                    prompt->targetId   = 0;
                    work->selection.id = hs->id;
                    work->promptKind   = hs->promptKind;
                    task->state        = 4;
                    return;
                }
                hs++;
            } while (hs->id != -1);
        }
    } else {
        prompt->mode = 1;
    }
    if (work->field_51 == 0 && prompt->buttons[1].state == 2) {
        task->state = 6;
    }
}
