#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "decomp/common.h"
#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_underground_parking.h"

extern s32  func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern void func_80131E38(void);

extern UiObjectDesc  D_800611E4;
extern UiObject*     D_80067634;
extern McItemScan    D_80072724;
extern s8            D_8007272D;
extern UiObjectDesc  D_8010D80C;
extern RoomShopStock D_8010E138[];
extern UiObjectDesc  D_8010EFA0;
extern s8            D_80114C12;
extern s16           D_80114D08;
extern u32           D_80115694;
extern char          Gp_StrEmpty[];

/// Labels of the nine play-data rows, and the help line each row shows while
/// it is selected.
extern u8 D_shelter_b1_underground_parking_801868C4[];
extern u8 D_shelter_b1_underground_parking_801868CC[];
extern u8 D_shelter_b1_underground_parking_801868D0[];
extern u8 D_shelter_b1_underground_parking_801868D8[];
extern u8 D_shelter_b1_underground_parking_801868E4[];
extern u8 D_shelter_b1_underground_parking_801868F4[];
extern u8 D_shelter_b1_underground_parking_801868FC[];
extern u8 D_shelter_b1_underground_parking_80186904[];
extern u8 D_shelter_b1_underground_parking_8018690C[];
extern u8 D_shelter_b1_underground_parking_80186920[];
extern u8 D_shelter_b1_underground_parking_8018694C[];
extern u8 D_shelter_b1_underground_parking_80186970[];
extern u8 D_shelter_b1_underground_parking_801869A0[];
extern u8 D_shelter_b1_underground_parking_801869D4[];
extern u8 D_shelter_b1_underground_parking_80186A08[];
extern u8 D_shelter_b1_underground_parking_80186A40[];
extern u8 D_shelter_b1_underground_parking_80186A74[];
extern u8 D_shelter_b1_underground_parking_80186AAC[];

/// The unit suffix appended to the play-data counts.
extern u8 D_shelter_b1_underground_parking_80186914[];

/// The list the weapon and PE usage panels fill.
extern UiList D_shelter_b1_underground_parking_80186B10;

/// Title and list of the menu `func_shelter_b1_underground_parking_8017EDE8`
/// runs.
extern const char D_shelter_b1_underground_parking_8017D638[];
extern UiList     D_shelter_b1_underground_parking_80186B98;

/// List of the menu `func_shelter_b1_underground_parking_8017F2A0` runs.
extern UiList D_shelter_b1_underground_parking_80186AE8;

/// Texts the menu's four row handlers draw.
extern u8 D_shelter_b1_underground_parking_8018689C[];
extern u8 D_shelter_b1_underground_parking_801868A4[];
extern u8 D_shelter_b1_underground_parking_801868B0[];
extern u8 D_shelter_b1_underground_parking_801868BC[];

/// Descriptors of the panels the rows open.
extern UiObjectDesc D_shelter_b1_underground_parking_80186B50;
extern UiObjectDesc D_shelter_b1_underground_parking_80186B6C;

/// The 0xFFFF-terminated item id lists `func_shelter_b1_underground_parking_8017F80C`
/// chooses from.
extern u16 D_shelter_b1_underground_parking_80186BBC[];
extern u16 D_shelter_b1_underground_parking_80186BC4[];
extern u16 D_shelter_b1_underground_parking_80186BCC[];
extern u16 D_shelter_b1_underground_parking_80186BD4[];
extern u16 D_shelter_b1_underground_parking_80186BE4[];
extern u16 D_shelter_b1_underground_parking_80186BF4[];
extern u16 D_shelter_b1_underground_parking_80186C04[];
extern u16 D_shelter_b1_underground_parking_80186C0C[];
extern u16 D_shelter_b1_underground_parking_80186C1C[];
extern u16 D_shelter_b1_underground_parking_80186C2C[];
extern u16 D_shelter_b1_underground_parking_80186C3C[];
extern u16 D_shelter_b1_underground_parking_80186C44[];
extern u16 D_shelter_b1_underground_parking_80186C58[];
extern u16 D_shelter_b1_underground_parking_80186C70[];
extern u16 D_shelter_b1_underground_parking_80186C84[];
extern u16 D_shelter_b1_underground_parking_80186C8C[];
extern u16 D_shelter_b1_underground_parking_80186C9C[];
extern u16 D_shelter_b1_underground_parking_80186CB4[];
extern u16 D_shelter_b1_underground_parking_80186CC8[];
extern u16 D_shelter_b1_underground_parking_80186CD0[];
extern u16 D_shelter_b1_underground_parking_80186CE4[];
extern u16 D_shelter_b1_underground_parking_80186D00[];
extern u16 D_shelter_b1_underground_parking_80186D10[];
extern u16 D_shelter_b1_underground_parking_80186D1C[];
extern u16 D_shelter_b1_underground_parking_80186D34[];
extern u16 D_shelter_b1_underground_parking_80186D50[];
extern u16 D_shelter_b1_underground_parking_80186D64[];
extern u16 D_shelter_b1_underground_parking_80186D6C[];
extern u16 D_shelter_b1_underground_parking_80186D80[];
extern u16 D_shelter_b1_underground_parking_80186DA0[];
extern u16 D_shelter_b1_underground_parking_80186DB0[];
extern u16 D_shelter_b1_underground_parking_80186DBC[];
extern u16 D_shelter_b1_underground_parking_80186DD4[];
extern u16 D_shelter_b1_underground_parking_80186DD8[];
extern u16 D_shelter_b1_underground_parking_80186DDC[];
extern u16 D_shelter_b1_underground_parking_80186DE4[];
extern u16 D_shelter_b1_underground_parking_80186DF4[];
extern u16 D_shelter_b1_underground_parking_80186DFC[];
extern u16 D_shelter_b1_underground_parking_80186E04[];
extern u16 D_shelter_b1_underground_parking_80186E0C[];
extern u16 D_shelter_b1_underground_parking_80186E18[];
extern u16 D_shelter_b1_underground_parking_80186E20[];
extern u16 D_shelter_b1_underground_parking_80186E2C[];
extern u16 D_shelter_b1_underground_parking_80186E34[];
extern u16 D_shelter_b1_underground_parking_80186E40[];
extern u16 D_shelter_b1_underground_parking_80186E4C[];
extern u16 D_shelter_b1_underground_parking_80186E54[];
extern u16 D_shelter_b1_underground_parking_80186E5C[];
extern u16 D_shelter_b1_underground_parking_80186E68[];
extern u16 D_shelter_b1_underground_parking_80186E74[];
extern u16 D_shelter_b1_underground_parking_80186E7C[];
extern u16 D_shelter_b1_underground_parking_80186E88[];
extern u16 D_shelter_b1_underground_parking_80186E94[];
extern u16 D_shelter_b1_underground_parking_80186EA0[];
extern u16 D_shelter_b1_underground_parking_80186EA4[];
extern u16 D_shelter_b1_underground_parking_80186EB0[];
extern u16 D_shelter_b1_underground_parking_80186EBC[];
extern u16 D_shelter_b1_underground_parking_80186EC8[];
extern u16 D_shelter_b1_underground_parking_80186ED0[];
extern u16 D_shelter_b1_underground_parking_80186EDC[];
extern u16 D_shelter_b1_underground_parking_80186EE8[];
extern u16 D_shelter_b1_underground_parking_80186EF4[];
extern u16 D_shelter_b1_underground_parking_80186EFC[];
extern u16 D_shelter_b1_underground_parking_80186F08[];
extern u16 D_shelter_b1_underground_parking_80187098[];

/// Texts and panel descriptors of the shop list's two special rows (ids
/// 0xFFFE and 0xFFFC) and of the panel a bought item opens.
extern u8           D_shelter_b1_underground_parking_80186FD0[];
extern u8           D_shelter_b1_underground_parking_80186FE0[];
extern u8           D_shelter_b1_underground_parking_80186FE4[];
extern UiObjectDesc D_shelter_b1_underground_parking_80187148;
extern UiObjectDesc D_shelter_b1_underground_parking_8018719C;

/// The shop's unlockable stock rows.
extern RoomShopTier D_shelter_b1_underground_parking_80186F14[13];

/// The shop list's row handlers and the balance panel beside it.
extern UiListItemFunc D_shelter_b1_underground_parking_8018709C[];
extern UiObjectDesc   D_shelter_b1_underground_parking_801871B8;

/// Texts of the four rows that pick an entry of the shop's id list, and the
/// panel they open.
extern u8           D_shelter_b1_underground_parking_80187020[];
extern u8           D_shelter_b1_underground_parking_80187028[];
extern u8           D_shelter_b1_underground_parking_80187034[];
extern u8           D_shelter_b1_underground_parking_8018703C[];
extern UiObjectDesc D_shelter_b1_underground_parking_80187110;

/// List of the menu `func_shelter_b1_underground_parking_80180C90` runs, and
/// the panel it opens first.
extern UiList       D_shelter_b1_underground_parking_801870A4;
extern UiObjectDesc D_shelter_b1_underground_parking_8018712C;

/// The purchase confirmation: its text and the panels it answers with.
extern u8           D_shelter_b1_underground_parking_80186FB4[];
extern UiObjectDesc D_shelter_b1_underground_parking_80187164;
extern UiObjectDesc D_shelter_b1_underground_parking_801871D4;

/// The three messages the notice panel picks from.
extern u8 D_shelter_b1_underground_parking_80187044[];
extern u8 D_shelter_b1_underground_parking_80187058[];
extern u8 D_shelter_b1_underground_parking_80187068[];

/// The charge panel's title, and the quantity and item map of the slot it is
/// animating.
extern u8         D_shelter_b1_underground_parking_8017D774[];
extern s32        D_shelter_b1_underground_parking_8018D744;
extern GpItemMap* D_shelter_b1_underground_parking_8018D748;

/// Label of the held-quantity line.
extern u8 D_shelter_b1_underground_parking_80187088[];

/// Text the quantity picker draws beside the item.
extern u8 D_shelter_b1_underground_parking_80187094[];

/// Text of the row that closes its panel.
extern u8 D_shelter_b1_underground_parking_80186FC8[];

/// The list `func_shelter_b1_underground_parking_80181D88` drives.
extern UiList D_shelter_b1_underground_parking_801870D0;

/// The descriptor of the modal panel `func_shelter_b1_underground_parking_80181EB0`
/// runs.
extern UiObjectDesc D_shelter_b1_underground_parking_801870F4;

/// The scene sub-task the cutscene runner spawned, while it runs.
extern Task* D_shelter_b1_underground_parking_8018D754;

/// The area records applied when the scene hands the Dryfield story on.
extern GpAreaApplyRec D_shelter_b1_underground_parking_80188888;

extern s32             D_shelter_b1_underground_parking_8018D758;
extern RoomCutsceneRec D_shelter_b1_underground_parking_8018D75C;
extern TaskDesc        D_shelter_b1_underground_parking_80187200;
extern TaskDesc        D_shelter_b1_underground_parking_80187260[];
extern TaskDesc        D_shelter_b1_underground_parking_8018726C[];
extern GpFadeWork      D_shelter_b1_underground_parking_8018D750;

/// The room's ambience table, one entry per area.
extern RoomAmbienceEntry D_shelter_b1_underground_parking_8018761C[];

extern s32 D_shelter_b1_underground_parking_801872D8;
extern s32 D_shelter_b1_underground_parking_801873DC;
extern s32 D_shelter_b1_underground_parking_80187544;

/// The task spawned from `D_shelter_b1_underground_parking_80187670`, and its
/// descriptor.
extern Task*    D_shelter_b1_underground_parking_8018D74C;
extern TaskDesc D_shelter_b1_underground_parking_80187670;

extern GpMsgEntry D_shelter_b1_underground_parking_80187230[];

extern DVECTOR        D_shelter_b1_underground_parking_801876D4[];
extern u8             D_shelter_b1_underground_parking_8018D788;
extern u8             D_shelter_b1_underground_parking_8018D789;
extern TaskDesc       D_shelter_b1_underground_parking_80187664[];
extern OverlayHotspot D_shelter_b1_underground_parking_8018767C[];
extern u8             D_shelter_b1_underground_parking_801876C4[];

extern SVECTOR D_shelter_b1_underground_parking_80187714[];
extern SVECTOR D_shelter_b1_underground_parking_8018771C[];
extern SVECTOR D_shelter_b1_underground_parking_80187724[];
extern SVECTOR D_shelter_b1_underground_parking_80187784[];
extern SVECTOR D_shelter_b1_underground_parking_801877A4[];

s32  func_shelter_b1_underground_parking_80183124(RoomEventMsg* in, RoomEventMsg* out);
void func_shelter_b1_underground_parking_80183810(Task* arg0);
void func_shelter_b1_underground_parking_8018414C(s32 x, s32 y, s32 variant);
void func_shelter_b1_underground_parking_80184304(Task* task);
void func_shelter_b1_underground_parking_801843F0(Task* task);
void func_shelter_b1_underground_parking_80184468(Task* task);
void func_shelter_b1_underground_parking_80184594(Task* task);
void func_shelter_b1_underground_parking_801845F8(Task* task);
void func_shelter_b1_underground_parking_801846EC(Task* arg0);
void func_shelter_b1_underground_parking_80184778(Task* task);
void func_shelter_b1_underground_parking_801847D0(Task* task);

void func_shelter_b1_underground_parking_8017DA50(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_shelter_b1_underground_parking_80186920,
                D_shelter_b1_underground_parking_8018694C,
                D_shelter_b1_underground_parking_80186970,
                D_shelter_b1_underground_parking_801869A0,
                D_shelter_b1_underground_parking_801869D4,
                D_shelter_b1_underground_parking_80186A08,
                D_shelter_b1_underground_parking_80186A40,
                D_shelter_b1_underground_parking_80186A74,
                D_shelter_b1_underground_parking_80186AAC,
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
            func_8002E53C(&req, D_shelter_b1_underground_parking_801868C4);
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
            func_8002E53C(&req, D_shelter_b1_underground_parking_801868F4);
            Text_ItoaUnsigned(p, Mc_SaveData.saveCount);
            Text_Strcat(p, D_shelter_b1_underground_parking_80186914);
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
            func_8002E53C(&req, D_shelter_b1_underground_parking_801868CC);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CC);
            Text_Strcat(p, D_shelter_b1_underground_parking_80186914);
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
            func_8002E53C(&req, D_shelter_b1_underground_parking_801868D0);
            Text_ItoaUnsigned(p, Mc_SaveData.field_6CE);
            Text_Strcat(p, D_shelter_b1_underground_parking_80186914);
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
            func_8002E53C(&req, D_shelter_b1_underground_parking_801868D8);
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
            Text_Strcat(p, D_shelter_b1_underground_parking_8018691C);
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
            func_8002E53C(&req, D_shelter_b1_underground_parking_801868E4);
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
            Text_Strcat(p, D_shelter_b1_underground_parking_8018691C);
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
            func_8002E53C(&req, D_shelter_b1_underground_parking_801868FC);
            Text_ItoaUnsigned(p, Mc_SaveData.clearCount);
            Text_Strcat(p, D_shelter_b1_underground_parking_80186914);
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
            func_8002E53C(&req, D_shelter_b1_underground_parking_80186904);
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
            func_8002E53C(&req, D_shelter_b1_underground_parking_8018690C);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, Mc_SaveData.field_930), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// Title of the play-data menu.
const char D_shelter_b1_underground_parking_8017D610[] = "Play Data";

/// Drawn in place of a usage percentage that reached 100.
const u8 D_shelter_b1_underground_parking_8017D61C[] = "100.0%";

void func_shelter_b1_underground_parking_8017E21C(DialogPrompt* arg0, UiObject* arg1)
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
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_shelter_b1_underground_parking_8017D61C, arg0->field_1C, 3, 2);
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
        Text_Strcat(p, D_shelter_b1_underground_parking_8018691C);
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
void func_shelter_b1_underground_parking_8017E618(UiList* list, UiObject* obj)
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

/// PE counterpart of `func_shelter_b1_underground_parking_8017E618`: fills the
/// "Play Data" PE-usage panel's `RoomPeUsage` block from the save's per-slot
/// use counters.
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
void func_shelter_b1_underground_parking_8017E914(UiList* list, UiObject* obj)
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
const char D_shelter_b1_underground_parking_8017D624[] = "Weapon Data";
const char D_shelter_b1_underground_parking_8017D630[] = "PE Data";

void func_shelter_b1_underground_parking_8017EC34(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_shelter_b1_underground_parking_80186B10;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_shelter_b1_underground_parking_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_shelter_b1_underground_parking_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B34, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_shelter_b1_underground_parking_8017E618(list, obj);
        } else {
            func_shelter_b1_underground_parking_8017E914(list, obj);
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

/// Title of the menu below. The two bytes after its terminator are not zero,
/// so it stays assembly.
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking", D_shelter_b1_underground_parking_8017D638);

void func_shelter_b1_underground_parking_8017EDE8(Task* task)
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
    list          = &D_shelter_b1_underground_parking_80186B98;
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
        Ui_DrawText((UiPanel*)obj, D_shelter_b1_underground_parking_8017D638);
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

void func_shelter_b1_underground_parking_8017F0E0(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_shelter_b1_underground_parking_8017F7D0;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void func_shelter_b1_underground_parking_8017F13C(u8* str, s32 decimals)
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
u8* func_shelter_b1_underground_parking_8017F1AC(u8* buf, s32 value, s32 decimals)
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

    Text_Strcat(buf, D_shelter_b1_underground_parking_8018691C);
    return buf;
}

void func_shelter_b1_underground_parking_8017F2A0(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b1_underground_parking_80186AE8;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_shelter_b1_underground_parking_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B34, 0, 0, 1, obj);
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
void func_shelter_b1_underground_parking_8017F390(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
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

void func_shelter_b1_underground_parking_8017F494(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_8018689C, prompt->field_1C, 1, 0);
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

void func_shelter_b1_underground_parking_8017F578(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_801868A4, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B50, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_shelter_b1_underground_parking_8017F640(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_801868B0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B6C, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

void func_shelter_b1_underground_parking_8017F708(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_801868BC, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80186B6C, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
void func_shelter_b1_underground_parking_8017F7D0(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
/// `Mc_SaveData.gameMode` picks one of two sets of lists (below 2, or 2 and
/// up); within a set the low half of `mode` picks the list group (0x20, 0x21,
/// 0x30-0x33 and 0x40 each have one, every other value shares one) and the
/// high half the entry within it (0-3). A high half outside 0-3 gets a single
/// fallback list.
u16* func_shelter_b1_underground_parking_8017F80C(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186C9C;
                    case 1:
                        return D_shelter_b1_underground_parking_80186CB4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186CC8;
                    case 3:
                        return D_shelter_b1_underground_parking_80186CD0;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186CE4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186D00;
                    case 2:
                        return D_shelter_b1_underground_parking_80186D10;
                    case 3:
                        return D_shelter_b1_underground_parking_80186D1C;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186D34;
                    case 1:
                        return D_shelter_b1_underground_parking_80186D50;
                    case 2:
                        return D_shelter_b1_underground_parking_80186D64;
                    case 3:
                        return D_shelter_b1_underground_parking_80186D6C;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186D80;
                    case 1:
                        return D_shelter_b1_underground_parking_80186DA0;
                    case 2:
                        return D_shelter_b1_underground_parking_80186DB0;
                    case 3:
                        return D_shelter_b1_underground_parking_80186DBC;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186BE4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186BF4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186C04;
                    case 3:
                        return D_shelter_b1_underground_parking_80186C0C;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186C58;
                    case 1:
                        return D_shelter_b1_underground_parking_80186C70;
                    case 2:
                        return D_shelter_b1_underground_parking_80186C84;
                    case 3:
                        return D_shelter_b1_underground_parking_80186C8C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186C1C;
                    case 1:
                        return D_shelter_b1_underground_parking_80186C2C;
                    case 2:
                        return D_shelter_b1_underground_parking_80186C3C;
                    case 3:
                        return D_shelter_b1_underground_parking_80186C44;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186BBC;
                    case 1:
                        return D_shelter_b1_underground_parking_80186BC4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186BCC;
                    case 3:
                        return D_shelter_b1_underground_parking_80186BD4;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E68;
                    case 1:
                        return D_shelter_b1_underground_parking_80186E74;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E7C;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E88;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E94;
                    case 1:
                        return D_shelter_b1_underground_parking_80186EA0;
                    case 2:
                        return D_shelter_b1_underground_parking_80186EA4;
                    case 3:
                        return D_shelter_b1_underground_parking_80186EB0;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186EBC;
                    case 1:
                        return D_shelter_b1_underground_parking_80186EC8;
                    case 2:
                        return D_shelter_b1_underground_parking_80186ED0;
                    case 3:
                        return D_shelter_b1_underground_parking_80186EDC;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186EE8;
                    case 1:
                        return D_shelter_b1_underground_parking_80186EF4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186EFC;
                    case 3:
                        return D_shelter_b1_underground_parking_80186F08;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186DF4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186DFC;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E04;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E0C;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E40;
                    case 1:
                        return D_shelter_b1_underground_parking_80186E4C;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E54;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E5C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E18;
                    case 1:
                        return D_shelter_b1_underground_parking_80186E20;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E2C;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E34;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186DD4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186DD8;
                    case 2:
                        return D_shelter_b1_underground_parking_80186DDC;
                    case 3:
                        return D_shelter_b1_underground_parking_80186DE4;
                }
                break;
        }
    }
    return D_shelter_b1_underground_parking_80187098;
}

void func_shelter_b1_underground_parking_8017FE7C(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq   req;
    u8            buf[0x20];
    RoomShopList* shop;
    McItemScan*   scan;
    s32           y;
    s32           scaled;
    UiObject*     child;
    UiObject*     child2;
    s32           blocked;
    s32           status;
    s32           itemId;
    s32           price;

    shop    = (RoomShopList*)obj->owner->work;
    blocked = 0;
    itemId  = shop->items[prompt->field_8];
    /* &D_80072724 hoisted into a saved register here, as the original does,
       instead of being rematerialised at the Gp_SumScanQty call. */
    scan = &D_80072724;
    if (prompt->field_C == 1) {
        D_shelter_b1_underground_parking_80186FB0 = itemId;
    }

    if (itemId == 0xFFFE) {
        status = obj->status;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam((s32)D_shelter_b1_underground_parking_80186FE4, 0, 0);
            }
        }
        if (Gp_HasMappedItem() == 0) {
            prompt->field_1C = Ui_LookupTable(obj, 2);
            prompt->field_C  = 0;
        }
        req.x          = obj->baseX + prompt->field_18;
        y              = obj->baseY - 4;
        req.y          = prompt->field_1A + y;
        req.otIndex    = (s16)obj->drawOrder + 1;
        req.field_8    = prompt->field_1C;
        req.glyphTable = 0;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, D_shelter_b1_underground_parking_80186FD0);
        if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_8018719C, 0, 1, 1, obj);
            obj->status = 0;
        }
        return;
    }

    if (itemId == 0xFFFC) {
        status = obj->status;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Ui_SetHolderParam((s32)Gp_StrEmpty, 0, 0);
            }
        }
        if (Gp_SumScanQty(scan, 0x8F) != 0) {
            blocked          = 1;
            prompt->field_1C = Ui_LookupTable(obj, 2);
        }
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_80186FE0, prompt->field_1C, 1, 0);
        if (prompt->field_C == 1 && blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            child = Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187148, itemId, 1, 1, obj);
            if (child != NULL) {
                Ui_ClampDialogRect((UiPanel*)child, (UiPanel*)prompt, (UiPanel*)obj);
                obj->status = 0;
            }
        }
        return;
    }

    price = Gp_ItemDescs[itemId].price;
    if (func_800B7420(itemId) != 0) {
        blocked          = 1;
        prompt->field_1C = Ui_LookupTable(obj, 2);
    }
    if (prompt->field_22 != 0x41) {
        status = obj->status;
        if (((status >> 16) == 1) || (status == 1)) {
            if (prompt->field_10 == prompt->field_8) {
                Gp_SetHolderItemText(itemId);
                Gp_SetPreviewItem(itemId, 0);
            }
        }
    }
    if (prompt->field_C == 1) {
        if (blocked == 0 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            child2 = Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187148, itemId, 1, 1, obj);
            if (child2 != NULL) {
                SndEvt_EnqueueType6(0x16, 0, 0);
                Ui_ClampDialogRect((UiPanel*)child2, (UiPanel*)prompt, (UiPanel*)obj);
                obj->status = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, itemId, 1, 1, obj);
            obj->status = 0;
        }
    }
    Gp_DrawItemLabel(obj, prompt->field_18, prompt->field_1A, itemId, prompt->field_1C, 0);
    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: emits the scaled index before the table base so the
           `addu` is index-first, matching the original. */
        scaled = itemId * 4;
        Gp_DrawQty(obj, prompt->field_18, prompt->field_1A, D_8010E138[itemId].perBuy, prompt->field_1C);
    }
    Text_ItoaUnsigned(buf, price);
    Text_DrawPrompt(obj, -prompt->field_18, prompt->field_1A, buf, prompt->field_1C, 3, 2);
}

/// Adds an item id to the room's shop list, keeping one entry per item kind:
/// ids 0xF..0x32 are three consecutive levels of the same kind, so an entry of
/// the same kind is overwritten only by a higher level.
void func_shelter_b1_underground_parking_80180308(RoomShopList* shop, UiObject* obj, s32 item)
{
    Task*         task = obj->owner;
    s32           mode = task->spawnArg1;
    RoomShopList* list = (RoomShopList*)task->work;
    s32           i;

    for (i = 0; i < shop->list.field_4; i++) {
        s32 cur = list->items[i];
        s32 q;

        if (cur == item) {
            return;
        }
        if (((mode & 0xFFFF) == 0x10) &&
            (((u32)(item - 0x9D) < 3U) || (item == 0x8A) || (item == 0x65))) {
            return;
        }
        if (((u32)(item - 0xF) < 0x24U) && ((u16)(cur - 0xF) < 0x24U)) {
            q = (item - 0xF) / 3;
            if ((q == (cur - 0xF) / 3) && (((item - 0xF) % 3 + 1) > ((cur - 0xF) % 3 + 1))) {
                list->items[i] = item;
                return;
            }
        }
    }

    Gp_SetItemSeenBit(item, 1);
    list->items[shop->list.field_4] = item;
    shop->list.field_4++;
}

/// Fills `shop` with the ids the shop currently offers, then sorts them by
/// `Gp_ItemSortKey` and caps the visible row count at 9.
///
/// The upper halfword of the owning task's `spawnArg1` is the shop's mode. It
/// picks the fixed id list and which items of each unlocked price row are
/// stocked: mode 0 takes 0x80-0x9F plus a handful of other ids, mode 1
/// 0xA0-0xBF, mode 2 0x60-0x7F and 0xD, and mode 3 everything from 1 to 0x5F
/// the other modes do not take, plus the twelve two-bit stock levels of
/// `Mc_SaveData.shopStock`. A demo save unlocks every row and every level.
void func_shelter_b1_underground_parking_80180454(RoomShopList* shop, UiObject* obj)
{
    RoomShopList* list;
    u16*          ids;
    s32           mode;
    s32           tier;
    s32           slot;
    s32           level;
    s32           id;
    s32           item;
    s32           unlocked;
    s32           i;
    s32           j;
    s32           k;
    s32           key;
    s32           otherKey;
    u16           tmp;
    u8            count;

    mode = obj->owner->spawnArg1;
    ids  = func_shelter_b1_underground_parking_8017F80C(mode);

    shop->list.field_4 = 0;
    while (*ids != 0xFFFF) {
        func_shelter_b1_underground_parking_80180308(shop, obj, *ids);
        ids++;
    }

    if (Mc_SaveData.demoScene == 1) {
        Mc_SaveData.shopTiers = 0x1FFF;
        Mc_SaveData.shopStock = -1;
    }

    if (Mc_SaveData.gameMode == 0) {
        if (Mc_SaveData.shopTiers != 0) {
            for (tier = 0; tier < 13; tier++) {
                unlocked = Mc_SaveData.shopTiers & (1 << tier);
                if (unlocked != 0) {
                    for (j = 0; j < 3; j++) {
                        item = D_shelter_b1_underground_parking_80186F14[tier].items[j];
                        switch (mode >> 16) {
                            case 0:
                                if (((u32)(item - 0x80) < 0x20U) || (item == 0xC) || (item == 9) ||
                                    (item == 0xA) || (item == 0x46) || (item == 0x45) ||
                                    (item == 0x42) || (item == 0x43) || (item == 0x44)) {
                                    func_shelter_b1_underground_parking_80180308(shop, obj, item);
                                }
                                break;
                            case 1:
                                if ((u32)(item - 0xA0) < 0x20U) {
                                    func_shelter_b1_underground_parking_80180308(shop, obj, item);
                                }
                                break;
                            case 2:
                                if (((u32)(item - 0x60) < 0x20U) || (item == 0xD)) {
                                    func_shelter_b1_underground_parking_80180308(shop, obj, item);
                                }
                                break;
                            case 3:
                                if (((u32)(item - 1) < 0x5FU) && (item != 0xD) && (item != 0xC) &&
                                    (item != 9) && (item != 0xA) && (item != 0x46) &&
                                    (item != 0x45) && (item != 0x42) && (item != 0x43) &&
                                    (item != 0x44)) {
                                    func_shelter_b1_underground_parking_80180308(shop, obj, item);
                                }
                                break;
                        }
                    }
                }
            }
        }

        if ((mode >> 16) == 3) {
            for (slot = 0; slot < 0xC; slot++) {
                level = (Mc_SaveData.shopStock >> (slot * 2)) & 3;
                if (slot == 0 ? level >= 2 : level > 0) {
                    /* The assignment keeps `+ 0xE` on the level instead of
                       letting GCC reassociate it onto the row base. */
                    func_shelter_b1_underground_parking_80180308(shop, obj, slot * 3 + (id = level + 0xE));
                }
            }
        }
    }

    list = (RoomShopList*)obj->owner->work;
    for (i = 0; i < shop->list.field_4 - 1; i++) {
        key = Gp_ItemSortKey(list->items[i]);
        for (k = i + 1; k < shop->list.field_4; k++) {
            otherKey = Gp_ItemSortKey(list->items[k]);
            if (otherKey < key) {
                tmp            = list->items[i];
                key            = otherKey;
                list->items[i] = list->items[k];
                list->items[k] = tmp;
            }
        }
    }

    count              = shop->list.field_4;
    shop->list.field_5 = count;
    if ((s8)count >= 0xA) {
        shop->list.field_5 = 9;
    }
    D_shelter_b1_underground_parking_80186FB0 = -1;
}

/// Title of the shop list.
const u8 D_shelter_b1_underground_parking_8017D750[] = "Select";

/// Label drawn ahead of the BP balance and the purchase total.
const u8 D_shelter_b1_underground_parking_8017D758[] = "BP";

void func_shelter_b1_underground_parking_80180820(Task* task)
{
    TextDrawReq   req;
    UiObject*     obj;
    RoomShopList* shop;
    Task*         head;
    Task*         child;
    Task*         next;
    UiObject*     childObj;
    void*         mem;
    s32           code;
    s32           x;
    s32           y;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)D_shelter_b1_underground_parking_8017D750);
    if (task->state == 0) {
        mem  = memCalloc(sizeof(RoomShopList), 0);
        shop = mem;
        if (mem != NULL) {
            /* Keeps the allocation's own register distinct from the tested one,
               so the null test stays on $v0 and the copy fills its delay slot. */
            SOFT_TOUCH_REG(shop);
            task->work         = (TaskIdMap*)shop;
            shop->list.funcs   = D_shelter_b1_underground_parking_8018709C;
            shop->list.field_6 = 0;
            shop->list.field_7 = 0xF;
            func_shelter_b1_underground_parking_80180454(shop, obj);
            Ui_LayoutListPanel(&shop->list, (UiPanel*)obj);
            shop->list.field_A = 1;
            Ui_SetListScrollFlag(&shop->list, 1);
            obj->field_12      += 8;
            shop->list.field_17 = 8;
            Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_801871B8, 0, 0, 0, obj);
            task->state += 1;
        }
    }
    Ui_UpdateListNoAnim(task->work, obj);
    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, (s16)obj->field_18 + 6);

    x              = obj->baseX - 2;
    req.x          = obj->field_1E + x;
    y              = obj->baseY + 2;
    req.y          = obj->field_18 + y;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = 0x606060;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 1;
    func_8002E53C(&req, D_shelter_b1_underground_parking_8017D758);

    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        }
    }

    head = task->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2;
            code     = childObj->field_2E;
            next     = child->nextSibling;
            if (code != -1) {
                if (code == 6) {
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->status = 1;
                }
            } else {
                obj->field_2E = code;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

void func_shelter_b1_underground_parking_80180A70(DialogPrompt* prompt, UiObject* obj)
{
    u8* text;
    s32 status;
    s32 one;
    s32 one2;

    if ((prompt->field_4 - 1) == prompt->field_8) {
        one = 1;
        Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b1_underground_parking_80186FC8, prompt->field_1C, one, 0);
        if (prompt->field_C == one && Pad_CheckButtons(0, one, Pad_MaskConfirm) != 0) {
            obj->field_2E = 6;
        }
        return;
    }

    text                  = D_shelter_b1_underground_parking_80187020;
    obj->owner->spawnArg1 = (u16)obj->owner->spawnArg1;
    switch (prompt->field_8) {
        case 0:
            break;
        case 1:
            text                   = D_shelter_b1_underground_parking_80187028;
            obj->owner->spawnArg1 |= 0x10000;
            break;
        case 2:
            text                   = D_shelter_b1_underground_parking_80187034;
            obj->owner->spawnArg1 |= 0x20000;
            break;
        case 3:
            text                   = D_shelter_b1_underground_parking_8018703C;
            obj->owner->spawnArg1 |= 0x30000;
            break;
    }

    if (*func_shelter_b1_underground_parking_8017F80C(obj->owner->spawnArg1) == 0xFFFF) {
        prompt->field_1C = Ui_LookupTable(obj, 2);
        prompt->field_C  = 0;
    }

    one2 = 1;
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, text, prompt->field_1C, one2, 0);

    status = obj->status;
    if (((status >> 16) == one2) || (status == one2)) {
        if (prompt->field_10 == prompt->field_8) {
            Ui_SetHolderParam((s32)Gp_StrEmpty, 0, 0);
        }
    }

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187110, obj->owner->spawnArg1, 1, 1, obj);
        obj->status = 0;
    }
}

/// Title of the menu below.
const u8 D_shelter_b1_underground_parking_8017D75C[] = "List";

void func_shelter_b1_underground_parking_80180C90(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2;
    list          = &D_shelter_b1_underground_parking_801870A4;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)D_shelter_b1_underground_parking_8017D75C);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_8018712C, task->spawnArg1, 0, 1, obj);
        Ui_SpawnFromDesc(&D_8010D80C, 0, 0, 0, obj);
        list->field_4 = 5;
        list->field_5 = 5;
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0) {
        obj->field_2E = -1;
    }

    head = task->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2;
            code     = childObj->field_2E;
            next     = child->nextSibling;
            if (code != -1) {
                if (code == 6) {
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->status = 1;
                }
            } else {
                Wip_UiHolder  = NULL;
                obj->field_2E = code;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

/// Label of the balance panel's item-count line.
const u8 D_shelter_b1_underground_parking_8017D764[] = "TOTAL";

void func_shelter_b1_underground_parking_80180E38(Task* task)
{
    s8            digits[0x20];
    s8            total[0x20];
    TextDrawReq   req0;
    TextDrawReq   req1;
    UiObject*     obj;
    PlayerStatus* cfg;
    McItemScan*   scan;
    s8*           p;
    s32           x;
    s32           y;
    s32           y2;
    s32           col;
    s32           capacity;
    s32           count;

    obj = task->spawnArg2;
    cfg = &Player_Status;
    x   = (s16)obj->field_1C + 2;
    col = (s16)obj->field_1E - 2;
    y   = (s16)obj->field_18;

    req0.x          = obj->baseX + x;
    req0.y          = obj->baseY + y + 9;
    req0.otIndex    = (s16)obj->drawOrder + 1;
    req0.field_8    = 0x606060;
    req0.glyphTable = 5;
    req0.centerMode = 0;
    req0.field_E    = 1;
    func_8002E53C(&req0, D_shelter_b1_underground_parking_8017D758);

    Text_ItoaUnsigned((u8*)digits, cfg->bp);
    Text_DrawPrompt(obj, col, y + 0x19, (u8*)digits, 0x606060, 3, 2);

    y2              = y + 0x28;
    req1.x          = obj->baseX + x;
    req1.y          = obj->baseY + (y2 - 6);
    req1.otIndex    = (s16)obj->drawOrder + 1;
    req1.field_8    = 0x606060;
    req1.glyphTable = 5;
    req1.centerMode = 0;
    req1.field_E    = 1;
    func_8002E53C(&req1, (char*)D_shelter_b1_underground_parking_8017D764);

    p        = total;
    scan     = &Mc_SaveData.carriedItems;
    count    = Gp_CountScanItems(scan);
    capacity = scan->rowCount;
    Text_ItoaUnsigned((u8*)p, count);
    while (*p != 0) {
        p++;
    }
    *p = '/';
    Text_ItoaUnsigned((u8*)(p + 1), capacity);
    Text_DrawPrompt(obj, col, y2 + 0xA, (u8*)total, 0x606060, 3, 2);
}

void func_shelter_b1_underground_parking_8018101C(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq   req;
    UiObject*     child;
    PlayerStatus* cfg;
    McItemScan*   scan;
    s32           itemId;
    s32           mode;
    s32           price;

    itemId = obj->owner->spawnArg1;

    req.x          = obj->baseX + (u16)prompt->field_18;
    req.y          = obj->baseY + (u16)prompt->field_1A;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_shelter_b1_underground_parking_80186FB4);

    mode = prompt->field_C;
    if (mode == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        cfg   = &Player_Status;
        price = Gp_ItemDescs[itemId].price;
        scan  = &D_80072724;
        SndEvt_EnqueueType6(0x16, 0, 0);
        if (cfg->bp >= price) {
            if (Gp_CanAddItem(scan, itemId) == 0) {
                if ((u32)(itemId - 0xA0) < 0x20U && Gp_SumScanQty(scan, itemId) != 0) {
                    Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187164, 2, 1, 1, obj);
                } else {
                    Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187164, 1, 1, 1, obj);
                }
                obj->status = 0;
            } else if ((obj->owner->parent->spawnArg1 >> 16) == mode) {
                child = Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_801871D4, itemId, 1, 1, obj);
                if (child != NULL) {
                    Ui_ClampDialogRect((UiPanel*)child, (UiPanel*)prompt, (UiPanel*)obj);
                    obj->status = 0;
                }
            } else {
                cfg->bp -= price;
                Gp_GiveItem(scan, itemId, -1);
                obj->field_2E = 6;
            }
        } else {
            Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_80187164, 0, 1, 1, obj);
            obj->status = 0;
        }
    }
}

/// Title of the notice panel.
const u8 D_shelter_b1_underground_parking_8017D76C[] = "Notice";

void func_shelter_b1_underground_parking_80181230(Task* task)
{
    UiObject* obj;
    u8*       text;
    s32       kind;

    kind = task->spawnArg1;
    obj  = task->spawnArg2;
    switch (kind) {
        case 1:
            text = D_shelter_b1_underground_parking_80187058;
            break;
        case 2:
            text = D_shelter_b1_underground_parking_80187068;
            break;
        default:
            text = D_shelter_b1_underground_parking_80187044;
            break;
    }

    Ui_DrawText((UiPanel*)obj, (char*)D_shelter_b1_underground_parking_8017D76C);
    obj->field_2E = 0;
    if (task->state == 0) {
        Ui_SizeFromTextPlain((UiPanel*)obj, text);
        task->killCountdown = 0xBC;
        task->state        += 1;
    }
    Text_DrawMultiLine(obj, (s16)obj->field_1C + 2, (s16)obj->field_18 + 0xF, text, 0x606060, 1, 0);
    task->killCountdown -= gDisplayState.frameTicks;
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
            return;
        }
        if (task->killCountdown <= 0 || Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            ((UiObject*)task->parent->spawnArg2)->field_2E = 6;
            task->killCountdown                            = 0x7FFF;
        }
    }
}

/// Title of the charge panel; the byte after its terminator is not zero, so
/// it stays assembly.
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking", D_shelter_b1_underground_parking_8017D774);

/// The charge station's transfer panel: steps through the mapped item slots,
/// takes the slot's current level as the bar's starting value and its related
/// quantity as the target, then animates the bar up to it over 0xBC frames.
/// Confirm or cancel (or the timer running out) advances to the next slot;
/// running out of slots reports code 6 to the parent.
void func_shelter_b1_underground_parking_801813B0(Task* task)
{
    UiObject*   obj;
    GpItemMap*  map;
    McItemSlot* slot;
    s32         slotId;
    s32         itemId;
    s32         curItem;
    s32         relItem;
    s32         qty;
    s32         y;
    s32         h;
    s32         status;
    s16         countdown;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)D_shelter_b1_underground_parking_8017D774);

    if (task->state == 0) {
        task->spawnArg1 = 0;
        task->state     = task->state + 1;
    }
    if (task->state == 1) {
        slotId          = Gp_NextMappedSlot(task->spawnArg1);
        task->spawnArg1 = slotId;
        if (slotId < 0) {
            obj->field_2E = 6;
        } else {
            map                                       = Gp_GetItemMap(slotId);
            D_shelter_b1_underground_parking_8018D748 = map;
            itemId                                    = map->field_1;
            slot                                      = Gp_GetItemSlot(itemId);
            if (D_shelter_b1_underground_parking_8018D748->field_0 == 0) {
                D_shelter_b1_underground_parking_8018D744 = slot->ammoQty;
                slot->ammoQty                             = Gp_GetRelatedQty(itemId, 0);
            } else {
                D_shelter_b1_underground_parking_8018D744 = slot->attachQty;
                slot->attachQty                           = Gp_GetRelatedQty(itemId, 1);
            }
            task->killCountdown                         = 0xBC;
            D_shelter_b1_underground_parking_8018D744 <<= 8;
            task->state                                 = task->state + 1;
        }
    }

    curItem = D_shelter_b1_underground_parking_8018D748->field_1;
    relItem = D_shelter_b1_underground_parking_8018D748->field_2;
    if (D_shelter_b1_underground_parking_8018D748->field_0 == 0) {
        qty = Gp_GetRelatedQty(curItem, 0);
    } else {
        qty = Gp_GetRelatedQty(curItem, 1);
    }
    qty                                      <<= 8;
    D_shelter_b1_underground_parking_8018D744 += 0x40;
    if (qty < D_shelter_b1_underground_parking_8018D744) {
        D_shelter_b1_underground_parking_8018D744 = qty;
    }

    y = (s16)obj->field_18;
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0xF, curItem, 0x606060, 0);
    Ui_DrawHBar((UiPanel*)obj, (s16)obj->field_1C, (s16)obj->field_1E, y + 0x12);
    Gp_DrawItemLabel(obj, (s16)obj->field_1C + 2, y + 0x23, relItem, 0x606060, 0);
    Gp_DrawQty(obj, (s16)obj->field_1C + 2, y + 0x23, D_shelter_b1_underground_parking_8018D744 >> 8, 0x606060);
    h = (s16)obj->field_1A;
    func_800C0E20((UiPanel*)obj, (s16)obj->field_1C + 2, (s16)obj->field_1E - 2, h - 6, qty,
                  D_shelter_b1_underground_parking_8018D744, 0x1741F);

    if (task->state == 2) {
        countdown           = task->killCountdown - 1;
        task->killCountdown = countdown;
        status              = obj->status;
        if (status == 1 && (countdown <= 0 || Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskConfirm) != 0)) {
            task->state     = status;
            task->spawnArg1 = task->spawnArg1 + 1;
        }
    }
}

/// Shop quantity row: the "Amount" label at the preview slot plus how many of
/// the previewed item the player already holds. Stackable items (0xA0..0xBF)
/// ask the scan for their stack quantity; everything else is counted by
/// walking the item table.
///
/// `guard` is the register the loop's entry test reads. The target compares a
/// copy of `count` (`move s4,s3` in the branch delay slot) rather than the
/// loop counter, and everything that keeps GCC 2.8.1 on that shape is
/// codegen-only:
///  - the dead `guard = 0` after the last draw makes `guard`, not `count`,
///    the cse-canonical zero, so the duplicated exit test is rewritten onto it;
///  - the two soft uses give it four references: with two, local-alloc moves a
///    single-use constant init next to its use (into the else block, past the
///    label, where the `move` from `count` can no longer be formed), and with
///    three it colours after `item` (`$s5`) instead of before (`$s4`).
void func_shelter_b1_underground_parking_80181678(Task* task)
{
    u8          buf[0x10];
    TextDrawReq req;
    UiObject*   obj;
    s32         item;
    s32         y;
    s32         ry;
    s32         count;
    s32         guard;
    s32         i;
    s32         n;
    McItemRec*  rec;

    item         = D_shelter_b1_underground_parking_80186FB0;
    obj          = task->spawnArg2;
    task->status = 0;
    if ((CdCmd_IsIdle() & 0xFFFF) && D_shelter_b1_underground_parking_80186FB0 == Gp_GetPreviewItem()) {
        func_800C7AE8(obj, obj->field_1C + 2, (s16)obj->field_18 + 2, 0x20);
    } else {
        func_800C7AE8(obj, obj->field_1C + 2, (s16)obj->field_18 + 2, 0x120);
    }
    y = (s16)obj->field_18 + 0x50;
    if (item < 0x100) {
        req.x          = obj->field_1C + (obj->baseX + 2);
        ry             = obj->baseY - 6;
        req.y          = ry + y;
        req.otIndex    = (s16)obj->drawOrder + 1;
        req.glyphTable = 5;
        req.field_8    = 0x606060;
        req.centerMode = 0;
        req.field_E    = 1;
        func_8002E53C(&req, D_shelter_b1_underground_parking_80187088);
        count = 0;
        guard = 0;
        if ((u32)(item - 0xA0) < 0x20U) {
            count = Gp_ScanStackQty(&D_80072724, item);
        } else {
            rec = Gp_GetItemTable(&D_80072724) + D_80072724.firstRow;
            n   = D_80072724.rowCount;
            SOFT_USE_REG2(guard, guard);
            for (i = 0; i < n; i++) {
                if (rec[i].itemId == item) {
                    count++;
                }
            }
        }
        Text_DrawPrompt(obj, (s16)obj->field_1E - 2, y + 0xA, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
        guard = 0;
    }
}

void func_shelter_b1_underground_parking_8018184C(Task* task)
{
    u8          buf[0x20];
    TextDrawReq req;
    UiObject*   obj;
    UiObject*   parentObj;
    s32         itemId;
    s32         price;
    s32         maxQty;
    s32         scaled;
    s32         afford;
    s32         held;
    /* The stock ceiling stays in $v0, so the scan count the shop just fetched
       has to be copied out of the return register instead of coalescing into
       it. */
    register s32 maxHeld asm("v0");
    s32          count;
    s32          left;
    s32          top;
    s32          x;
    s32          y;
    s32          i;

    itemId = task->spawnArg1;
    obj    = task->spawnArg2;
    maxQty = 1;
    price  = Gp_ItemDescs[itemId].price;

    if (task->state == 0) {
        task->extraState = 1;
        Ui_UpdateLayoutSize((UiPanel*)obj, 0, Ui_Scale15(3) - 3);
        task->state = task->state + 1;
    }

    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: emits the scaled index before the table base so the
           `addu` is index-first, matching the original. */
        scaled = itemId * 4;
        if (D_8010E138[itemId].perBuy != 0) {
            held    = Gp_ScanStackQty(&D_80072724, itemId);
            maxHeld = D_8010E138[itemId].maxHeld;
            maxQty  = maxHeld - held;
            if (maxQty <= 0) {
                maxQty = 1;
            } else {
                maxQty = (maxQty - 1) / D_8010E138[itemId].perBuy;
                maxQty = maxQty + 1;
            }
        }
    } else {
        maxQty = D_80072724.rowCount - Gp_CountScanItems(&D_80072724);
    }

    afford = Player_Status.bp / price;
    if (afford < maxQty) {
        maxQty = afford;
    }

    left = (s16)obj->field_1C;
    x    = left + 2;
    top  = (s16)obj->field_18;
    y    = top + 0xF;
    Gp_DrawItemLabel(obj, x, y, itemId, 0x606060, 0);
    if ((u32)(itemId - 0xA0) < 0x20) {
        /* Dead: same index-first ordering as above. */
        scaled = itemId * 4;
        Gp_DrawQty(obj, x, y, D_8010E138[itemId].perBuy, 0x606060);
    }

    count = task->extraState;
    Text_DrawPrompt(obj, left + 0x98, y, D_shelter_b1_underground_parking_80187094, 0x606060, 3, 2);
    Text_DrawPrompt(obj, -x, y, Text_ItoaSigned(buf, count), 0x606060, 3, 2);
    Ui_DrawHBar((UiPanel*)obj, left, -x + 2, top + 0x12);

    req.x          = obj->baseX - x;
    y              = top + 0x1A;
    req.y          = obj->baseY + y;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = 0x606060;
    req.glyphTable = 5;
    req.centerMode = 2;
    req.field_E    = 1;
    func_8002E53C(&req, D_shelter_b1_underground_parking_8017D758);

    Text_DrawPrompt(obj, -x, top + 0x2B, Text_ItoaSigned(buf, count * price), 0x606060, 3, 2);

    if (obj->status == 1) {
        parentObj = task->parent->spawnArg2;
        if (Pad_CheckButtons(0, 1, 0x3000) != 0) {
            if (task->extraState < maxQty) {
                task->extraState = task->extraState + 1;
                SndEvt_EnqueueType6(0x15, 0, 0);
            }
        } else if (Pad_CheckButtons(0, 1, 0xC000) != 0) {
            if (task->extraState >= 2) {
                task->extraState = task->extraState - 1;
                SndEvt_EnqueueType6(0x15, 0, 0);
            }
        } else if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            Player_Status.bp -= price * task->extraState;
            for (i = 0; i < task->extraState; i++) {
                Gp_GiveItem(&D_80072724, itemId, -1);
            }
            SndEvt_EnqueueType6(0x16, 0, 0);
            parentObj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            parentObj->field_2E = 6;
        }
    }
}

void func_shelter_b1_underground_parking_80181CCC(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->baseX + (u16)prompt->field_18;
    req.y          = obj->baseY + (u16)prompt->field_1A;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_shelter_b1_underground_parking_80186FC8);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

void func_shelter_b1_underground_parking_80181D88(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_shelter_b1_underground_parking_801870D0;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }

    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        code     = childObj->field_2E;
        if (code != -1) {
            if (code == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->status = 1;
            }
        } else {
            obj->field_2E = -1;
        }
    }
}

/// Runs a modal panel over the room: takes the prim buffer and stops the frame
/// timer while the panel is up, waits for the panel to report closed, then
/// gives both back and ends the stage.
void func_shelter_b1_underground_parking_80181EB0(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_shelter_b1_underground_parking_801870F4, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        gGameSession->uiOpen = 1;
        task->spawnArg2      = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            Ui_TeardownTree(obj, obj->owner);
            task->killCountdown = 10;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            gGameSession->uiOpen = 0;
            taskKill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

/// The departure task, carrying out `D_shelter_b1_underground_parking_8018D77C`.
/// State 0 sends the departure's halfword to the slot-3 game pointer as message
/// 0x3EE, or skips to state 2 when it is 0xFFFF; state 1 polls the pointer with
/// 0x3F0 until it answers 0. States 2 and 3 play the departure's sound event and
/// wait for the voice to go quiet (0 skips both). State 4 commits the save
/// location the departure names, re-spawns the player task as type 0x11 and
/// kills itself.
void func_shelter_b1_underground_parking_80181FE4(Task* arg0)
{
    GpXformArg msg;
    void*      slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            /* Read unsigned, though the staging code stores it signed. */
            msg.rot.vy = (u16)D_shelter_b1_underground_parking_8018D77C.facing;
            if (msg.rot.vy == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_shelter_b1_underground_parking_8018D77C.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_shelter_b1_underground_parking_8018D77C.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_shelter_b1_underground_parking_8018D77C.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.stage = D_shelter_b1_underground_parking_8018D77C.stage;
            Mc_SaveData.at4.loc.area  = D_shelter_b1_underground_parking_8018D77C.area;
            Mc_SaveData.at4.loc.warp  = D_shelter_b1_underground_parking_8018D77C.warp;
            Mc_SaveData.at4.loc.room  = D_shelter_b1_underground_parking_8018D77C.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// The room's cutscene runner: suppresses the player and ally HUD, loads and
/// starts the scene's caption slot, lets confirm or cancel cut the sub-task
/// short, applies the story-flag side effects when the scene ends, and
/// restores everything before killing itself.
void func_shelter_b1_underground_parking_80182154(Task* task)
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
            D_shelter_b1_underground_parking_8018D754 = NULL;
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
            D_shelter_b1_underground_parking_8018D754 = Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_shelter_b1_underground_parking_8018D754);
                task->state++;
            } else if (Task_PollKill(D_shelter_b1_underground_parking_8018D754, &killOut) != 0) {
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
                        Gp_ApplyAreaRecs(&D_shelter_b1_underground_parking_80188888);
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

/// Starts caption slot 0xA and spawns entry 4 of
/// `D_shelter_b1_underground_parking_8018726C` when the player asks for it.
///
/// The player must not be aiming (`field_954 != 2`), captions must be idle,
/// the session room must be 7 or later, and the model root must stand with X
/// below -0x1266 and Z inside [-0x7CF, 0x7D0), with `D_80114C12 != 1` and
/// `D_80071075` clear. Then the 0x1000 pad mask with the yaw in the 0x3FF-wide
/// window opening at 0xA01, or the 0x4000 mask with it in the window at 0x201,
/// takes the weapon away and runs the handoff.
void func_shelter_b1_underground_parking_801826C0(void)
{
    Task*      task;
    GameActor* actor;
    GpCoord*   coord;
    s32        z;
    s32        facing;

    task  = gameGetPtrSlot(3);
    actor = (GameActor*)task->work;
    coord = task->extra.tmd->coords;
    if ((actor->field_954 != 2) && (Gp_CapBusy() == 0) && (gGameSession->at4.loc.room >= 7) &&
        (coord->coord.t[0] < -0x1266)) {
        z = coord->coord.t[2];
        if (z < 0x7D0) {
            if ((z >= -0x7CF) && (D_80114C12 != 1) && (gDisplayState.pendingMode == 0)) {
                facing = (u16)actor->field_52 & 0xFFF;
                if (Pad_CheckButtons(0, 0, 0x1000) != 0) {
                    if ((u32)(facing - 0xA01) < 0x3FFU) {
                        Gp_MsgPlayerWeapon(0);
                        Gp_StartCapSlot(0xA, 0, 1);
                        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                    }
                }
                if ((Pad_CheckButtons(0, 0, 0x4000) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                    Gp_MsgPlayerWeapon(0);
                    Gp_StartCapSlot(0xA, 0, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                }
            }
        }
    }
}

/// The three states of the room's main task, run by
/// `func_shelter_b1_underground_parking_801838B4`: set-up, the per-frame
/// handler, and the kill.
const TaskFuncTable3 D_shelter_b1_underground_parking_8017D7F4 = {
    {
        func_shelter_b1_underground_parking_80183810,
        func_shelter_b1_underground_parking_801826C0,
        taskKill,
    },
};

/// Room event handler keyed on `msg->field_2`: 1 calls `func_80131E38` in
/// place 0x15, 0xA starts caption slot 0xA and sets nibble 0x1B4 to 2 while
/// the room is below 7, and 0xB / 0xC pick a caption or spawn per room.
s32 func_shelter_b1_underground_parking_80182830(Task* task, s32 msgId, RoomEventMsg* msg)
{
    if (msg->field_2 == 1 && gGameSession->at4.loc.place == 0x15) {
        func_80131E38();
    }
    if (msg->field_2 == 0xA) {
        if ((u8)msg->field_3 == 1 && gGameSession->at4.loc.room < 7) {
            Gp_StartCapSlot(0xA, 1, 0);
            GameFlag_SetNibble(0x1B4, 2);
        }
    }
    if (msg->field_2 == 0xB) {
        switch (gGameSession->at4.loc.room) {
            case 2:
                Gp_RunCapCmd1(8);
                break;
            case 3:
                Gp_RunCapCmd1(0xF);
                break;
            case 4:
                Gp_RunCapCmd1(9);
                break;
            case 5:
                Gp_MsgPlayerWeapon(0);
                Gp_RunCapCmd1(0xC);
                Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 1, 0, 0);
                break;
            case 6:
            case 7:
            case 8:
                Gp_RunCapCmd1(0xE);
                break;
        }
    }
    if (msg->field_2 == 0xC) {
        switch (gGameSession->at4.loc.room) {
            case 6:
                Gp_StartCapSlot(0xB, 1, 0);
                break;
            case 7:
                if (D_shelter_b1_underground_parking_8018D758 != 0) {
                    Gp_RunCapCmd1(0x1E);
                } else if (GameFlag_GetNibble(0x7A) < 6) {
                    Gp_MsgPlayerWeapon(0);
                    Gp_StartCapSlot(0xB, 1, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 3, 0, 0);
                } else {
                    Gp_StartCapSlot(0xB, 1, 2);
                }
                break;
            case 8:
                Gp_StartCapSlot(0xB, 1, 2);
                break;
        }
    }
    return 0;
}

s32 func_shelter_b1_underground_parking_80182A60(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    RoomCutsceneRec* st;

    switch (arg2) {
        case 1:
            if (gGameSession->at4.loc.room < 6) {
                if (GameFlag_GetNibble(0xC7) == 0) {
                    Gp_RunCapCmd1(1);
                } else if (GameFlag_GetNibble(0xE7) == 0) {
                    if (GameFlag_GetNibble(0xE8) == 0) {
                        Gp_RunCapCmd1(2);
                    } else {
                        Gp_MsgPlayerWeapon(0);
                        Gp_MsgPlayer3F3(0);
                        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                    }
                } else {
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgPlayer3F3(0);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                }
            } else {
                Gp_RunCapCmd1(0x10);
            }
            break;
        case 7:
            switch (gGameSession->at4.loc.room) {
                case 1:
                    Gp_RunCapCmd1(7);
                    break;
                case 2:
                    Gp_RunCapCmd1(8);
                    break;
                case 3:
                    Gp_RunCapCmd1(0xF);
                    break;
                case 4:
                    Gp_RunCapCmd1(9);
                    break;
                case 6:
                case 7:
                case 8:
                    Gp_RunCapCmd1(0xE);
                    break;
            }
            break;
        case 4:
            switch (gGameSession->at4.loc.room) {
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                    Gp_RunCapCmd1(4);
                    break;
                case 6:
                    Gp_RunCapCmd1(5);
                    break;
                case 7:
                    Gp_RunCapCmd1(6);
                    break;
                case 8:
                    Gp_RunCapCmd1(0x10);
                    break;
            }
            break;
        case 2:
        case 3:
        case 5:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, arg2, 0);
            break;
        case 13:
            st           = &D_shelter_b1_underground_parking_8018D75C;
            st->field_4  = 0x5414000B;
            st->field_8  = 0x5414000E;
            st->field_10 = 0x5414000C;
            st->field_C  = 0x5414000D;
            st->field_0  = 0x14;
            if (D_shelter_b1_underground_parking_8018D758 == 0) {
                if (GameFlag_GetNibble(0x158) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    GameFlag_SetNibble(0x158, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 1, 0);
                } else {
                    st->field_1 = 1;
                    st->field_3 = 1;
                    st->field_2 = 0;
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 0, 9, (s32)st);
                }
            } else {
                D_shelter_b1_underground_parking_8018D758 = 0;
                st->field_1                               = 0x1F;
                st->field_3                               = 0;
                st->field_2                               = 1;
                Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 0, arg2, (s32)st);
            }
            break;
        case 22:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_80187260, 0, arg2, 0);
            break;
        case 48:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 4, 0);
            break;
    }
    return 0;
}

/// Task body that waits for the caption to finish, then on caption key 0xB
/// spawns the 0x31 task and after 30 frames publishes
/// `D_shelter_b1_underground_parking_8018D77C` and spawns entry 0 of
/// `D_shelter_b1_underground_parking_80187200`. Any other key restores the
/// weapon and ends the task.
void func_shelter_b1_underground_parking_80182DB4(Task* task)
{
    RoomDeparture  rec;
    RoomEventMsg   msg;
    RoomDeparture* p;
    s32            (*handler)(RoomEventMsg*, RoomEventMsg*);

    switch (task->state) {
        case 0:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 1:
            if (Gp_GetCapEventKey() == 0xB) {
                D_shelter_b1_underground_parking_8018D750.field_0 = 0;
                D_shelter_b1_underground_parking_8018D750.field_1 = 0;
                D_shelter_b1_underground_parking_8018D750.field_2 = 0x1E;
                Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b1_underground_parking_8018D750);
                task->killCountdown = 0x1E;
                task->state++;
            } else {
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
        case 2:
            if (task->killCountdown == 0) {
                if (GameFlag_GetNibble(0x4B) == 0xA) {
                    GameFlag_SetNibble(0x4B, 9);
                }
                if (GameFlag_GetNibble(0x11F) == 1) {
                    GameFlag_SetNibble(0x11F, 2);
                    D_8007272D = 0x1B;
                }
                handler      = func_shelter_b1_underground_parking_80183124;
                rec.stage    = 5;
                rec.area     = 1;
                rec.room     = 1;
                rec.warp     = 1;
                rec.sndEvent = 0x54140008;
                rec.facing   = -1;
                Gp_MsgPlayerWeapon(0);
                p           = &rec;
                msg.msgId   = p->area;
                msg.field_2 = p->warp;
                msg.field_3 = p->room;
                msg.field_5 = 0;
                handler(&msg, &msg);
                p->area                                   = msg.msgId;
                p->warp                                   = msg.field_2;
                p->room                                   = msg.field_3;
                D_shelter_b1_underground_parking_8018D77C = rec;
                Task_SpawnFromTable(&D_shelter_b1_underground_parking_80187200, 0, 0, 0);
                taskKill(task);
            }
            task->killCountdown--;
            break;
    }
}

/// Keeps the room's looping ambience in step with the area the session is in:
/// `gGameSession->at4.loc.view` selects an entry of the ambience table, and
/// state 0 starts the loop with `SndEvt_EnqueueType6`. Once
/// `D_shelter_b1_underground_parking_8018D758` is clear, state 1 queues a
/// `SndEvt_EnqueueType7` event for the loop and ends the task; otherwise it waits for the session's view to stop matching `Mc_SaveData.at4.loc.view`,
/// states 2 to 4 walk the task along, and state 5 retunes the loop to the new
/// entry with `SndEvt_EnqueueTypeA` and returns to state 1.
void func_shelter_b1_underground_parking_80182FC8(Task* task)
{
    s32 pan;
    s32 vol;
    u8  idx;

    idx = gGameSession->at4.loc.view;
    if (idx < 9) {
        pan = D_shelter_b1_underground_parking_8018761C[idx].pan;
        vol = D_shelter_b1_underground_parking_8018761C[idx].vol / 2;
    } else {
        pan = 0;
        vol = 0;
    }

    switch (task->state) {
        case 0:
            SndEvt_EnqueueType6(0x5414000F, (s8)pan, (s8)vol);
            task->state = task->state + 1;
            break;
        case 1:
            if (D_shelter_b1_underground_parking_8018D758 == 0) {
                func_shelter_b1_underground_parking_80186890(0);
                SndEvt_EnqueueType7(0x5414000F, 1);
                taskKill(task);
                break;
            }
            if (Mc_SaveData.at4.loc.view != gGameSession->at4.loc.view) {
                task->state = task->state + 1;
            }
            break;
        case 2:
        case 3:
        case 4:
            task->state = task->state + 1;
            break;
        case 5:
            SndEvt_EnqueueTypeA(0x5414000F, (s8)pan, (s8)vol);
            task->state = 1;
            break;
    }
}

/// Answers the marker query for one Neo Ark map room; the Neo Ark area map
/// carries the same body. Most rooms have no
/// marker; the five that do read a GameFlag nibble, either straight (plus one,
/// rooms 7 / 13 / 32) or folded into a fixed set of states (rooms 20 and 21).
s32 func_shelter_b1_underground_parking_80183124(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_5 == 0) {
        switch (in->msgId) {
            case 7:
                out->field_3 = GameFlag_GetNibble(0xE1) + 1;
                break;
            case 13:
                out->field_3 = GameFlag_GetNibble(0xD9) + 1;
                break;
            case 20:
                out->field_3 = 1;
                if (GameFlag_GetNibble(0xDD) != 0) {
                    if (GameFlag_GetNibble(0xDC) != 0) {
                        out->field_3 = 3;
                    } else {
                        out->field_3 = 2;
                    }
                }
                break;
            case 21:
                if (GameFlag_GetNibble(0xE9) != 0) {
                    out->field_3 = 4;
                } else {
                    out->field_3 = 1;
                }
                break;
            case 32:
                out->field_3 = GameFlag_GetNibble(0xDD) + 1;
                break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            default:
                break;
        }
    }
    return 1;
}

/// The examine task's eight states, run by
/// `func_shelter_b1_underground_parking_80184284`, from set-up to the closing
/// fade.
const TaskFuncTable8 D_shelter_b1_underground_parking_8017D9A4 = {
    {
        func_shelter_b1_underground_parking_80184304,
        func_shelter_b1_underground_parking_801843F0,
        func_shelter_b1_underground_parking_80184468,
        func_shelter_b1_underground_parking_80184594,
        func_shelter_b1_underground_parking_801845F8,
        func_shelter_b1_underground_parking_801846EC,
        func_shelter_b1_underground_parking_80184778,
        func_shelter_b1_underground_parking_801847D0,
    },
};

/// A one-shot sound cue on a timer: fires its spawn argument as a sound event
/// on the first frame, repeats it at frame 0x50 and kills itself at 0x78.
void func_shelter_b1_underground_parking_801831F4(Task* task)
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

s32 func_shelter_b1_underground_parking_80183284(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x121 || arg2 == 0x122) {
        if (gGameSession->at4.loc.room == 6) {
            node = Gp_PendingObj4C;
            while (node != NULL) {
                if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                    found = 1;
                    goto check;
                }
                node = node->next;
            }
            found = 0;
        check:
            if (found != 0) {
                Task_SpawnOnDefaultList(D_shelter_b1_underground_parking_8018726C, 2, 0, 0);
                gGameSession->hideHud    = 1;
                gGameSession->eventState = 1;
                return 1;
            }
        }
    }
    return 0;
}

s32 func_shelter_b1_underground_parking_80183360(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (D_shelter_b1_underground_parking_8018D758 == 0) {
        return 1;
    }
    if (in->field_5 == 0) {
        Gp_RunCapCmd1(0x1E);
    }
    return 2;
}

s32 func_shelter_b1_underground_parking_801833DC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x54140010, 0, 0);
    }
    return 0;
}

void func_shelter_b1_underground_parking_80183410(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(task->spawnArg1);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
        advance:
            task->state += 1;
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xF) {
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            } else {
                task->state = 0;
            }
            break;
    }
}

/// Spawns entry 0 of `D_shelter_b1_underground_parking_80187670` and kills
/// itself once that task has gone.
void func_shelter_b1_underground_parking_801834D4(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            D_shelter_b1_underground_parking_8018D74C = Task_SpawnFromTable(&D_shelter_b1_underground_parking_80187670, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_shelter_b1_underground_parking_8018D74C, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

void func_shelter_b1_underground_parking_80183560(Task* arg0)
{
    s32 state = arg0->state;

    switch (state) {
        case 0:
            if (Gp_CapBusy() == 0) {
                arg0->state += 1;
            }
            return;
        case 1:
            if (Gp_GetCapEventKey() == 0xB) {
                gGameSession->at4.loc.room  = 6;
                Mc_SaveData.at4.loc.room    = 6;
                gGameSession->roomObjsDirty = state;
                func_800E8614((s32)&D_shelter_b1_underground_parking_801872D8, 1);
                GameFlag_SetNibble(0xF4, 1);
                Gp_SetItemSeenBit(0x123, 1);
            } else {
                Gp_MsgPlayerWeapon(1);
            }
            taskKill(arg0);
            break;
    }
}

void func_shelter_b1_underground_parking_8018363C(Task* arg0)
{
    if (arg0->state == 0) {
        SetDispMask(0);
        D_80115768            = 1;
        gGameSession->hideHud = 1;
        func_800E8634((s32)&D_shelter_b1_underground_parking_801873DC, 0, (s32)&D_shelter_b1_underground_parking_80187544);
        GameFlag_SetNibble(0xF4, 2);
        GameFlag_SetNibble(0x1B4, 0);
        arg0->state += 1;
        return;
    }
    taskKill(arg0);
}

void func_shelter_b1_underground_parking_801836D8(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        Gp_MsgPlayerWeapon(1);
        taskKill(arg0);
    }
}

void func_shelter_b1_underground_parking_80183714(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x300, 0);
            Gp_RunCapCmd(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

void func_shelter_b1_underground_parking_801837D8(u8 arg0)
{
    gGameSession->at4.loc.room  = arg0;
    Mc_SaveData.at4.loc.room    = arg0;
    gGameSession->roomObjsDirty = 1;
    gGameSession->viewDirty     = 1;
}

/// Room script callback: latch this room's script argument into `D_80115768`.
void func_shelter_b1_underground_parking_80183804(u8 arg0)
{
    D_80115768 = arg0;
}

void func_shelter_b1_underground_parking_80183810(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_underground_parking_80187230;
    Game_SetPtrSlot(arg0, 7);
    func_shelter_b1_underground_parking_801848A4();
    if (gGameSession->at4.loc.place == 0x15) {
        Gp_MsgSlot4Chain(0, 1);
    }
    if ((GameFlag_GetNibble(0x7A) >= 6) && (GameFlag_GetNibble(0x123) == 0)) {
        GameFlag_SetNibble(0x123, 1);
        func_shelter_b1_underground_parking_8018390C();
    }
    arg0->state = arg0->state + 1;
}

/// Dispatches a task through the three-entry state table, copied onto the
/// stack first.
void func_shelter_b1_underground_parking_801838B4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_underground_parking_8017D7F4;
    sp.funcs[task->state](task);
}

void func_shelter_b1_underground_parking_8018390C(void)
{
    if (D_shelter_b1_underground_parking_8018D758 == 0) {
        D_shelter_b1_underground_parking_8018D758 = 1;
        func_shelter_b1_underground_parking_80186890(1);
        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 5, 0, 0);
    }
}

/// Outlines `rect` on screen in (`r`, `g`, `b`) with four unconnected flat
/// `LINE_F2`s -- top, right, bottom and left edge of the rectangle spanning
/// (`x`, `y`) to (`x + w`, `y + h`) -- each linked into `gGpuCurrentOt[1]`.
void func_shelter_b1_underground_parking_80183958(RoomRect* rect, u8 r, u8 g, u8 b)
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

void func_shelter_b1_underground_parking_80183B9C(void)
{
    if (D_shelter_b1_underground_parking_8018D789 & 8) {
        func_shelter_b1_underground_parking_801857E0(-0x46, 0x54, 7, 0xF00);
    }
    if (D_shelter_b1_underground_parking_8018D789 & 4) {
        func_shelter_b1_underground_parking_801857E0(-0x28, 0x54, 7, 0xF0);
    }
    if (D_shelter_b1_underground_parking_8018D789 & 2) {
        func_shelter_b1_underground_parking_801857E0(-0xE, 0x54, 7, 0xF);
    }
    if (D_shelter_b1_underground_parking_8018D789 & 1) {
        func_shelter_b1_underground_parking_801857E0(0xC, 0x54, 7, 0xFF0);
    }
    func_shelter_b1_underground_parking_801857E0(0x11, -0x3D, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(0x3D, -0x3D, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(0x10, 9, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(-0x42, -0x19, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(-0x17, 0x2B, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(0x3D, 0x2B, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(
        D_shelter_b1_underground_parking_801876D4[D_shelter_b1_underground_parking_8018D789].vx,
        D_shelter_b1_underground_parking_801876D4[D_shelter_b1_underground_parking_8018D789].vy, 7, 0xF0);
}

void func_shelter_b1_underground_parking_80183CEC(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28 + port;
        pad    = (PadState*)&Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons[0].state;
        heldp  = &prompt->buttons[0].heldFrames;
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                           = 0;
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed = prompt->screen.packed;
                    *statep                                              = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_shelter_b1_underground_parking_8018414C(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues one 16x24 textured quad -- the room's on-screen action prompt icon --
/// at (`x`, `y`) into the head of the current OT. `variant` selects the palette,
/// 0x3C87 when it is 2 and 0x3C88 otherwise, and 0 draws nothing at all.
void func_shelter_b1_underground_parking_8018414C(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

/// Two-state dispatcher of the room's action-prompt task: builds the handler
/// pair on the stack and calls the entry `Task::state` names.
void func_shelter_b1_underground_parking_80184234(Task* task)
{
    TaskFunc states[2] = { func_shelter_b1_underground_parking_801848BC, func_shelter_b1_underground_parking_80183CEC };

    states[task->state](task);
}

/// The examine task: dispatches through its eight state handlers, copied onto
/// the stack first.
void func_shelter_b1_underground_parking_80184284(Task* task)
{
    TaskFuncTable8 fns;

    fns = D_shelter_b1_underground_parking_8017D9A4;
    fns.funcs[task->state](task);
}

void func_shelter_b1_underground_parking_80184304(Task* task)
{
    SbupExamineWork* st;
    OverlayHotspot*  hs;

    st = memCalloc(0x10, 0);
    if (st == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2          = Task_SpawnFromTable(D_shelter_b1_underground_parking_80187664, 0, 1, 0);
    task->work               = (TaskIdMap*)st;
    Mc_SaveData.at4.loc.view = 0x15;
    /* The once-loops fold away, but flow weights the references inside them
       by loop depth. The outer one keeps the state load below the mode store;
       the inner one lifts the work pointer's global-alloc priority back above
       the parameter's so the two keep their callee-saved homes. */
    do {
        task->state++;
        do {
            st->field_0 = 0;
        } while (0);
    } while (0);
    Display_AcquireRef();
    for (hs = D_shelter_b1_underground_parking_8018767C; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

void func_shelter_b1_underground_parking_801843F0(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    Gp_RunCapCmd(GameFlag_GetNibble(0xE7) == 0 ? 2 : 3, 0);
    task->state++;
}

void func_shelter_b1_underground_parking_80184468(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    OverlayHotspot*   hs     = D_shelter_b1_underground_parking_8018767C;
    SbupExamineWork*  work   = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    gGameSession->hideHud = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_shelter_b1_underground_parking_80184964(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->field_C    = hs->id;
                    work->promptKind = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

void func_shelter_b1_underground_parking_80184594(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    SbupExamineWork*  work   = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

void func_shelter_b1_underground_parking_801845F8(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    SbupExamineWork*  work   = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        if (work->field_C == 0x10) {
            SndEvt_EnqueueType6(0x54140004, 0, 0);
            if (D_shelter_b1_underground_parking_8018D788 != D_shelter_b1_underground_parking_8018D789) {
                if (gGameSession->at4.loc.room == 1) {
                    SndEvt_EnqueueType6(0x54140006, 0, 0);
                } else {
                    SndEvt_EnqueueType6(0x54140005, 0, 0);
                }
                task->state = 6;
                return;
            }
        } else {
            D_shelter_b1_underground_parking_8018D789 ^= work->field_C;
            SndEvt_EnqueueType6(0x54140004, 0, 0);
            task->state = 2;
            return;
        }
    }
    task->state = 2;
}

void func_shelter_b1_underground_parking_801846EC(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    Mc_SaveData.at4.loc.view   = 2;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    SOFT_BARRIER();
    taskKill((Task*)arg0->spawnArg2);
    Task_RequestKill(arg0, 0);
}

/// Commits the pending destination selected in the parking-lot map task:
/// promotes the pending value into the committed one, tears down the prompt
/// display, applies the selection to the session, then kills the child task
/// spawned for the selection UI and advances to the next state.
void func_shelter_b1_underground_parking_80184778(Task* task)
{
    D_shelter_b1_underground_parking_8018D788 = D_shelter_b1_underground_parking_8018D789;
    func_shelter_b1_underground_parking_80183B9C();
    func_shelter_b1_underground_parking_8018491C();
    task->killCountdown = 0;
    taskKill((Task*)task->spawnArg2);
    task->state++;
}

void func_shelter_b1_underground_parking_801847D0(Task* task)
{
    SbupExamineWork* work = (SbupExamineWork*)task->work;

    func_shelter_b1_underground_parking_80183B9C();
    work->fadeLevel += 6;
    if (work->fadeLevel >= 0x100) {
        work->fadeLevel = 0xFF;
    }
    Fade_DrawOverlay((u8)work->fadeLevel, (u8)work->fadeLevel, (u8)work->fadeLevel, 2);
    if (work->fadeLevel == 0xFF) {
        D_80114D08 = 0xA;
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        Display_ReleaseRef();
        gGameSession->eventState   = 0;
        gGameSession->hideHud      = 0;
        gGameSession->cutsceneHold = 0;
        Mc_SaveData.at4.loc.view   = 2;
        Task_RequestKill(task, 0);
    }
}

void func_shelter_b1_underground_parking_801848A4(void)
{
    D_shelter_b1_underground_parking_8018D788 = 0xFF;
    D_shelter_b1_underground_parking_8018D789 = 0;
}

/// Resets both action-prompt slots before a script's first cursor scan and steps
/// the caller on one state: clears each slot's leading words and its two
/// trailing shorts, parks the target id at 0x100 with `field_E` at 0xF, and
/// marks the slot as highlighted (`mode` 1).
void func_shelter_b1_underground_parking_801848BC(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0               = 0;
        prompt->field_4               = 0;
        prompt->targetId              = 0x100;
        prompt->field_E               = 0xF;
        prompt->buttons[0].heldFrames = 0;
        prompt->buttons[1].heldFrames = 0;
        prompt->mode                  = 1;
    }
    task->state = task->state + 1;
}

void func_shelter_b1_underground_parking_8018491C(void)
{
    Mc_SaveData.at4.loc.room    = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & 0xF];
    gGameSession->at4.loc.room  = D_shelter_b1_underground_parking_801876C4[D_shelter_b1_underground_parking_8018D788 & 0xF];
    gGameSession->roomObjsDirty = 1;
}

s32 func_shelter_b1_underground_parking_80184964(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

void func_shelter_b1_underground_parking_80184A18(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
        case 10:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            func_shelter_b1_underground_parking_8018543C(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_8018771C[11], 0x200, 0, 0x111);
            break;
        case 3:
        case 7:
        case 11:
        case 12:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            func_shelter_b1_underground_parking_8018543C(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_8018771C[11], 0x200, 0, 0x111);
            break;
        case 8:
        case 13:
            func_shelter_b1_underground_parking_80184C54(D_shelter_b1_underground_parking_801877A4, 0x200, 0, 0x210);
        case 4:
        case 14:
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_80187724[0], 0x200, -0x400, 0x111);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_80187724[2], 0x200, -0x400, 0x111);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_80187724[4], 0x200, 0x800, 0x111);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_80187724[6], 0x200, 0x800, 0x111);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_80187724[8], 0x200, 0, 0x111);
            break;
        case 9:
        case 15:
        case 22:
        case 24:
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_80187784[0], 0x200, 0, 0x111);
            func_shelter_b1_underground_parking_80184C54(&D_shelter_b1_underground_parking_80187784[2], 0x200, 0x800, 0x111);
            break;
        case 16:
            func_shelter_b1_underground_parking_8018543C(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            break;
        case 18:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                func_shelter_b1_underground_parking_80185A94(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            break;
        case 20:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                func_shelter_b1_underground_parking_80185F08(D_shelter_b1_underground_parking_80187714, 0x180, 0x80);
            } else {
                func_shelter_b1_underground_parking_80185F08(D_shelter_b1_underground_parking_80187714, 0x60, 0x80);
            }
            break;
    }
}

/// Projects the two world-space points `arg0[0]` and `arg0[1]` and, if the
/// second lies at an OTZ of at least 0x11, queues gouraud wedges around both
/// and between them, rotated by `arg2`; `arg1` is a signed half-extent. The
/// inner vertex is scaled by the frame-counter blend byte
/// `((field_8 & 1) * 8 | 0x20)`: red is `blend * ((arg3 << 16) >> 24)`, green
/// `blend * (((arg3 << 16) >> 20) & 1)`, blue `blend * (arg3 & 1)`.
void func_shelter_b1_underground_parking_80184C54(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 1);
        base      = (s16)arg2;
        b         = blend * (arg3 & 1);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            SCHED_BARRIER();
            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, if
/// the resulting OTZ is at least 0x11, queues four gouraud `POLY_G4` wedges
/// around the projected centre. `arg1` is a signed half-extent; the on-screen
/// radius is `(s16)arg1 * 64 / otz`. `arg2` scales the inner vertex by the
/// frame-counter blend byte `((field_8 & 1) * 8 | 0x20)`: red is
/// `blend * ((arg2 << 16) >> 24)`, green `blend * (((arg2 << 16) >> 20) & 1)`,
/// blue `blend * (arg2 & 1)`.
void func_shelter_b1_underground_parking_8018543C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    u8*                ds_ptr;
    DisplayState*      ds;
    s32                radius;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0xC);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw25Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        radius        = ((s16)arg1 * 64) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        ds_ptr        = (u8*)&gDisplayState;
        packed        = arg2 << 16;
        blend         = (((u8)((DisplayState*)ds_ptr)->animFrame & 1) * 8) | 0x20;
        r             = blend * (packed >> 24);
        g             = blend * ((packed >> 20) & 1);
        b             = blend * (arg2 & 1);
        ang           = 0;
        ds            = (DisplayState*)ds_ptr;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0xC);
}

void func_shelter_b1_underground_parking_801857E0(s16 x, s16 y, s16 radius, s16 color)
{
    POLY_G4* prim;
    s32      i;
    s32      ang;
    s32      t;
    u8       r;
    u8       g;
    u8       b;
    s32      base;
    s32      c;
    s32      rMask;
    s32      gMask;

    i     = 0;
    base  = (gDisplayState.animFrame & 1) * 12;
    c     = color;
    rMask = (c >> 4) & 0xF0;
    gMask = c & 0xF0;
    r     = base + rMask;
    g     = base + gMask;
    b     = base + ((color & 0xF) << 4);
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
            prim->x0 = x + ((radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = y + ((radius * rcos(ang)) >> 12);
            prim->x1 = x + ((radius * rsin(t)) >> 12);
            prim->y1 = y + ((radius * rcos(t)) >> 12);
            t        = ang + 0x200;
            prim->x2 = x;
            prim->y2 = y;
            prim->x3 = x + ((radius * rsin(t)) >> 12);
            prim->y3 = y + ((radius * rcos(t)) >> 12);
            ang      = t;
            addPrim((u_long*)(((((u32)0x40 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, 0x40);
        } while (ang < 0x1000);
        radius <<= 1;
        r      >>= 1;
        g      >>= 1;
        b      >>= 1;
        i++;
    } while (i < 3);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a signed
/// half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1` scales
/// `gDisplayState.animFrame` into `rsin` so the lit vertex pulses as
/// `rsin(...) / 34 + 0x78` on green and blue.
void func_shelter_b1_underground_parking_80185A94(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
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
        tmp     = (*scratch = head - 0x10);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
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
    SCRATCH_POP_BYTES(0x10);
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues a sixteen-wedge gouraud disc plus two
/// inner cross wedges around the projected centre. `arg2` is a signed
/// half-extent; on-screen radii are `(s16)arg2 * 64 / otz` (outer) and
/// `(s16)arg2 * 8 / otz` (inner). `arg1` scales `gDisplayState.animFrame` into
/// `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
void func_shelter_b1_underground_parking_80185F08(SVECTOR* arg0, s32 arg1, s32 arg2)
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

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
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

void func_shelter_b1_underground_parking_80186890(s16 arg0)
{
    D_shelter_b1_underground_parking_8018D78C = arg0;
}
