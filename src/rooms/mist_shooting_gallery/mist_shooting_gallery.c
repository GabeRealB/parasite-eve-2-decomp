#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

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
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8018055c.h"

#include "rooms/mist_shooting_gallery.h"

/// The four bonusGpGridParamsmode blurbs shown by the gallery's help panel, indexed by
/// `D_80072177`. `func_mist_shooting_gallery_8017FAE8` copies the whole thing
/// onto its stack before indexing it.
typedef struct MistShootingGalleryModeTexts {
    /* 0x0 */ u8* text[4];
} MistShootingGalleryModeTexts;
STATIC_ASSERT_SIZEOF(MistShootingGalleryModeTexts, 0x10);

/// One row of the gallery's DATA panel. `gauge` picks the bar string out of
/// `MistShootingGalleryGauges`; `label` is the word printed beside it
/// ("EASY", "GOOD", "RICH", ...).
typedef struct MistShootingGalleryRating {
    /* 0x0 */ s32 gauge;
    /* 0x4 */ u8* label;
} MistShootingGalleryRating;
STATIC_ASSERT_SIZEOF(MistShootingGalleryRating, 0x8);

/// The four ratings one DATA row can show, indexed by `Mc_SaveData::gameMode`
/// (the difficulty the save runs at). `func_mist_shooting_gallery_8017F128`
/// copies a whole table onto its stack before indexing it, like the mode table
/// above.
typedef struct MistShootingGalleryRatings {
    /* 0x00 */ MistShootingGalleryRating entries[4];
} MistShootingGalleryRatings;
STATIC_ASSERT_SIZEOF(MistShootingGalleryRatings, 0x20);

/// The six gauge strings the DATA panel draws through
/// `MistShootingGalleryRating::gauge`; also stackGpGridParamscopied before use.
typedef struct MistShootingGalleryGauges {
    /* 0x00 */ u8* bars[6];
} MistShootingGalleryGauges;
STATIC_ASSERT_SIZEOF(MistShootingGalleryGauges, 0x18);

/// One row of the gallery's RESULT panel: the points one kill of that target
/// is worth and the name printed beside it ("Red Target", "Crow", ...).
/// `func_mist_shooting_gallery_8017E234` walks the 13GpGridParamsentry table
/// `D_mist_shooting_gallery_80184F98` in step with the perGpGridParamstarget kill counts
/// in `MistShootingGalleryWork::pad_0F`.
typedef struct MistShootingGalleryTarget {
    /* 0x0 */ s32 points;
    /* 0x4 */ u8* name;
} MistShootingGalleryTarget;
STATIC_ASSERT_SIZEOF(MistShootingGalleryTarget, 0x8);

extern UiObjectDesc D_mist_shooting_gallery_80185060;
extern s32          D_mist_shooting_gallery_8018E0BC;
extern s32          D_mist_shooting_gallery_8018E0C0;

extern TaskDesc D_mist_shooting_gallery_80184F8C;
extern TaskDesc D_mist_shooting_gallery_801850D0;
extern s8       D_80072176;
extern s8       D_80072177;

extern u8 D_mist_shooting_gallery_8017D65C[]; // "TOTAL SCORE"

extern MistShootingGalleryModeTexts D_mist_shooting_gallery_8017D6D8;
extern MistShootingGalleryModeTexts D_mist_shooting_gallery_8017D708;
extern u8                           D_mist_shooting_gallery_8017D718[];
extern u8                           D_mist_shooting_gallery_8017D720[];
extern u8                           D_mist_shooting_gallery_8017D728[];
extern u8                           D_mist_shooting_gallery_8017D730[];

extern MistShootingGalleryRatings D_mist_shooting_gallery_8017D778;
extern MistShootingGalleryRatings D_mist_shooting_gallery_8017D7AC;
extern MistShootingGalleryRatings D_mist_shooting_gallery_8017D7E8;
extern MistShootingGalleryGauges  D_mist_shooting_gallery_8017D808;
extern char                       D_mist_shooting_gallery_8017D820[];
extern u8                         D_mist_shooting_gallery_8017D828[];
extern u8                         D_mist_shooting_gallery_8017D838[];
extern u8                         D_mist_shooting_gallery_8017D844[];
extern u8                         D_mist_shooting_gallery_8017D850[];

extern void     func_8002E53C(TextDrawReq* req, u8* text);
extern void     func_8014A398(void);
extern s32      func_8014AA54(GpSaveLoc* loc);
extern void     func_8014AB6C(void);
extern void     func_8014AF0C(void);
extern void     func_8014C5E0(s32, s32, s32);
extern s8       D_8007218B;
extern s8       D_8007272D;
extern s8       D_80072A92;
extern s8       D_80073BAE;
extern s16      D_8007A396;
extern s16      D_80114D08;
extern s32      D_8014D038;
extern TaskDesc D_8014E13C;
extern s32      D_80153274;
extern s32      D_80153D6C;

/// ScreenGpGridParamsfade "overlay owns the display" flag, first byte of the flag block
/// at 0x80071068. Declared as an array on purpose: GCC 2.8.1 exempts a
/// *fixedGpGridParamsaddress scalar* store from aliasing with a varyingGpGridParamsaddress struct
/// load, so a plain `extern s8` here lets the scheduler hoist the following
/// `arg0->state` load above the store. Indexing an array makes the store a
/// struct reference and keeps the two in order.
extern s8 D_80071068[];
extern s8 D_8007106B;

extern GpItemScan D_80072724;
extern u8         Gp_DebugAttachLevels[];

extern u8           D_80073BA9;
extern UiObjectDesc D_8010EFA0;

/// The ten weapons the gallery's weapon picker offers, in row order. Rows whose
/// item is not unlocked yet (`func_800B7420` returns 0) are skipped, so
/// `DialogPrompt::field_8` counts *drawn* rows, not table slots.
extern s16 D_mist_shooting_gallery_80184F34[];

extern UiList D_mist_shooting_gallery_80184F4C;

extern UiList                    D_mist_shooting_gallery_8018503C;
extern UiObjectDesc              D_mist_shooting_gallery_8018507C[];
extern UiObjectDesc              D_mist_shooting_gallery_8018501C;
extern MistShootingGalleryTarget D_mist_shooting_gallery_80184F98[];
extern GpMsgEntry                D_mist_shooting_gallery_801850E8[];
extern TaskDesc                  D_mist_shooting_gallery_801856B8;
extern TaskDesc                  D_mist_shooting_gallery_801850DC;
extern GpGridParams              D_mist_shooting_gallery_80185198;
extern GpGridParams              D_mist_shooting_gallery_801851F8;
extern GpGridParams              D_mist_shooting_gallery_80189968;

/// The jukebox's track lists, one per game mode, each a run of track id and
/// name pairs.
extern RoomsShared8018055cCourse D_mist_shooting_gallery_8018521C[];
extern RoomsShared8018055cCourse D_mist_shooting_gallery_80185234[];
extern RoomsShared8018055cCourse D_mist_shooting_gallery_8018524C[];
extern RoomsShared8018055cCourse D_mist_shooting_gallery_80185264[];
extern RoomsShared8018055cCourse D_mist_shooting_gallery_8018527C[];
extern RoomsShared8018055cCourse D_mist_shooting_gallery_80185294[];
extern RoomsShared8018055cCourse D_mist_shooting_gallery_801852B4[];
extern RoomsShared8018055cCourse D_mist_shooting_gallery_801852D4[];
extern RoomsShared8018055cCourse D_mist_shooting_gallery_801852F4[];
extern RoomsShared8018055cCourse D_mist_shooting_gallery_80185314[];

/// The jukebox menu's title, "SELECT". A stray 0xE1 byte follows its
/// terminator, so the block stays in assembly.
extern char D_mist_shooting_gallery_8017DB04[];

/// The jukebox's track list, whose row callback is
/// `func_mist_shooting_gallery_8018055C`.
extern UiList D_mist_shooting_gallery_80185338;

/// The jukebox panel's descriptor; its update routine is the menu task
/// `func_mist_shooting_gallery_80180728`.
extern UiObjectDesc D_mist_shooting_gallery_8018535C;

extern TaskDesc D_mist_shooting_gallery_80185378;
extern TaskDesc D_mist_shooting_gallery_80185384;

/// The room's active data bank pointer, and the two banks it chooses between.
extern void* D_mist_shooting_gallery_801853C0;
extern u32   D_mist_shooting_gallery_8018D1B4[];
extern u32   D_mist_shooting_gallery_8018DF38[];

extern SVECTOR D_mist_shooting_gallery_80185550[];
extern SVECTOR D_mist_shooting_gallery_80185570[];
extern SVECTOR D_mist_shooting_gallery_801855C0[];
extern SVECTOR D_mist_shooting_gallery_801855F0[];
extern SVECTOR D_mist_shooting_gallery_80185610[];
extern SVECTOR D_mist_shooting_gallery_80185670[];
extern SVECTOR D_mist_shooting_gallery_80185678[];
extern SVECTOR D_mist_shooting_gallery_80185680[];
extern SVECTOR D_mist_shooting_gallery_80185688[];
extern SVECTOR D_mist_shooting_gallery_80185690[];
extern SVECTOR D_mist_shooting_gallery_801856B0[];

void func_mist_shooting_gallery_801801E4(s32 arg0);
void func_mist_shooting_gallery_80181480(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_mist_shooting_gallery_80181CC4(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_mist_shooting_gallery_8017DCAC(s32 mode)
{
    GpItemScan* scan;
    u8*         levels;
    s32         row;
    s32         col;
    s32         i;
    s32         k;

    scan   = &D_80072724;
    row    = 0;
    levels = Gp_DebugAttachLevels;
    i      = row;
    for (; row < 4; row++, i += 3) {
        col = 0;
        k   = i;
        TOUCH_REG(k);
        do {
            *(u8*)((col + k) + (s32)levels) = 0;
            col++;
        } while (col < 3);
    }
    Gp_DebugAttachLevels[0] = 1;

    switch (mode) {
        case 1:
        case 2:
            break;
        case 3:
            Gp_SetItemSeenBit(0x40, 1);
            Gp_GiveItem(scan, 0x40, 1);
            break;
        case 4:
            Gp_SetItemSeenBit(0x40, 1);
            Gp_SetItemSeenBit(5, 1);
            Gp_GiveItem(scan, 0x40, 1)->attachSlot = 2;
            Gp_GiveItem(scan, 5, 1)->attachSlot    = 3;
            Gp_GiveItem(scan, 5, 1)->attachSlot    = 4;
            Gp_DebugAttachLevels[0xA]              = 1;
            Gp_DebugAttachLevels[1]                = 1;
            break;
        case 5:
            Gp_SetItemSeenBit(0x40, 1);
            Gp_SetItemSeenBit(1, 1);
            Gp_SetItemSeenBit(6, 1);
            Gp_GiveItem(scan, 0x40, 1);
            Gp_GiveItem(scan, 1, 1);
            Gp_GiveItem(scan, 1, 1);
            Gp_GiveItem(scan, 1, 1);
            Gp_GiveItem(scan, 6, 1);
            Gp_GiveItem(scan, 6, 1);
            Gp_DebugAttachLevels[0xA] = 1;
            Gp_DebugAttachLevels[1]   = 1;
            break;
    }
    Gp_FillHpMp();
    Gp_ApplyItemMap();
}
void func_mist_shooting_gallery_8017DE7C(DialogPrompt* arg0, UiObject* arg1)
{
    s32         item;
    s32         i;
    s32         skip;
    s32         status;
    s32         selected;
    s32         ammo;
    GpItemQty*  row;
    u8*         weaponIdx;
    GpItemScan* scan;

    item = 0;
    skip = arg0->field_8;
    i    = 0;
    do {
        if (func_800B7420(D_mist_shooting_gallery_80184F34[i]) != 0) {
            skip--;
            if (skip < 0) {
                item = D_mist_shooting_gallery_80184F34[i];
                break;
            }
        }
        i++;
    } while (i < 10);

    Gp_DrawItemLabel(arg1, arg0->field_18, arg0->field_1A, item, arg0->field_1C, 0);
    status = arg1->status;
    if (((status >> 16) == 1) || (status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
        }
    }
    selected = arg0->field_C;
    if (selected == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            scan       = &D_80072724;
            weaponIdx  = &D_80073BA9;
            row        = &Gp_QtyById0[item];
            ammo       = row->field_1;
            *weaponIdx = item - 0x7F;
            Gp_ResetScanDefault();
            Gp_ClearScanItems(scan);
            Gp_GiveItem(scan, item, 1);
            Gp_GiveItem(scan, 0x6C, 1);
            Gp_EquipMod(0x6C);
            Gp_GiveItem(scan, ammo, 0x3E7)->attachSlot = selected;
            Gp_EquipRelatedItem(scan, item, ammo, -1);
            Gp_FillHpMp();
            arg1->field_2E = 6;
            SndEvt_EnqueueType6(0x16, 0, 0);
        } else if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}
void func_mist_shooting_gallery_8017E090(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16*      weapon;
    s32       i;
    s32       count;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_mist_shooting_gallery_80184F4C;
    /* Only "Select" is drawn; the target names that follow it share this
     * literal because they are reached from a `.data` table of name/score
     * pairs, so nothing else in the room's C claims those rodata bytes. */
    Ui_DrawText((UiPanel*)obj,
                "Select\0\0Red Target\0\0Brown Target\0\0\0\0Yellow Target\0\0\0Woman\0\0\0Crow\0\0\0\0Bee\0"
                "Spider\0\0Snake\0\0\0Scorpion\0\0\0\0Rat\0Monkey\0\0Bear\0\0\0\0Bacterium");
    if (task->state == 0) {
        count  = 0;
        i      = count;
        weapon = D_mist_shooting_gallery_80184F34;
        do {
            if (func_800B7420(*weapon) != 0) {
                Gp_SetItemSeenBit(*weapon, 1);
                count += 1;
            }
            i++;
            weapon++;
        } while (i < 10);

        list->field_4 = count;
        if ((u8)count >= 0xB) {
            list->field_5 = 0xA;
        } else {
            list->field_5 = count;
        }
        list->field_10 = 0;
        list->field_9  = 0;
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        obj->field_C = -((s16)obj->field_10 / 2);
        obj->field_E = -((s16)obj->field_12 / 2);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        if (childObj->field_2E == -1 || childObj->field_2E == 6) {
            Ui_TeardownTree(childObj, childObj->owner);
            obj->status = 1;
        }
    }
}
INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D65C);

void func_mist_shooting_gallery_8017E234(Task* task)
{
    u8                       buf[0x20];
    TextDrawReq              req1;
    TextDrawReq              req2;
    TextDrawReq              req3;
    TextDrawReq              req4;
    TextDrawReq              req5;
    TextDrawReq              req6;
    MistShootingGalleryWork* work;
    s32                      rows;
    s32                      total;
    UiObject*                obj;
    s32                      i;
    s32                      kills;
    s32                      points;
    s32                      subtotal;
    s32                      xOff;
    s32                      y;
    s32                      status;
    s32                      state;
    s32                      bonus;
    s32                      flag;
    UiObject*                childObj;
    s32                      result;
    s32                      bottom1;
    s32                      bottom2;
    Task*                    child;

    i     = 0;
    rows  = 0;
    total = 0;
    obj   = task->spawnArg2;
    work  = (MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work;
    xOff  = (s16)obj->field_1C + 2;
    y     = (s16)obj->field_18 + 0x17;
    do {
        kills = (u8)work->pad_0F[i];
        if (kills > 0) {
            points = D_mist_shooting_gallery_80184F98[i].points;
            rows  += 1;

            req1.x          = obj->baseX + xOff;
            req1.y          = obj->baseY + y;
            req1.otIndex    = (s16)obj->drawOrder + 1;
            req1.field_8    = 0x606060;
            req1.glyphTable = 0;
            req1.centerMode = 0;
            req1.field_E    = 3;
            func_8002E53C(&req1, D_mist_shooting_gallery_80184F98[i].name);

            req2.x          = obj->baseX + 0x6E + xOff;
            req2.y          = obj->baseY + y;
            req2.otIndex    = (s16)obj->drawOrder + 1;
            req2.field_8    = 0x606060;
            req2.glyphTable = 0;
            req2.centerMode = 2;
            req2.field_E    = 3;
            func_8002E53C(&req2, Text_ItoaSigned(buf, points));

            req3.x          = obj->baseX + 0x91 + xOff;
            req3.y          = obj->baseY + y;
            req3.otIndex    = (s16)obj->drawOrder + 1;
            req3.field_8    = 0x606060;
            req3.glyphTable = 0;
            req3.centerMode = 2;
            req3.field_E    = 3;
            func_8002E53C(&req3, Text_ItoaSigned(buf, kills));
            subtotal = kills * points;

            req4.x          = obj->baseX - 5 - xOff;
            req4.y          = obj->baseY + y;
            req4.otIndex    = (s16)obj->drawOrder + 1;
            req4.field_8    = 0x606060;
            req4.glyphTable = 0;
            req4.centerMode = 2;
            req4.field_E    = 3;
            func_8002E53C(&req4, Text_ItoaSigned(buf, subtotal));
            total += subtotal;
            y     += 0xB;
        }
        i += 1;
    } while (i < 0xD);

    Ui_DrawHBar((UiPanel*)obj, xOff, -xOff, (s16)obj->field_1A - 0xE);

    req1.x          = obj->baseX + 0x78 + xOff;
    bottom1         = obj->baseY - 6;
    req1.y          = obj->field_1A + bottom1;
    req1.otIndex    = (s16)obj->drawOrder + 1;
    req1.field_8    = 0x606060;
    req1.glyphTable = 5;
    req1.centerMode = 2;
    req1.field_E    = 1;
    func_8002E53C(&req1, D_mist_shooting_gallery_8017D65C);

    req2.x          = obj->baseX - 5 - xOff;
    bottom2         = obj->baseY - 4;
    req2.y          = obj->field_1A + bottom2;
    req2.otIndex    = (s16)obj->drawOrder + 1;
    req2.field_8    = 0x606060;
    req2.glyphTable = 0;
    req2.centerMode = 2;
    req2.field_E    = 3;
    func_8002E53C(&req2, Text_ItoaSigned(buf, total));

    Ui_DrawHBar((UiPanel*)obj, xOff, -xOff, (s16)obj->field_18 + 0xA);

    y               = (s16)obj->field_18 + 6;
    req3.x          = obj->baseX + 0x1E + xOff;
    req3.y          = obj->baseY + y;
    req3.otIndex    = (s16)obj->drawOrder + 1;
    req3.field_8    = 0x606060;
    req3.glyphTable = 5;
    req3.centerMode = 1;
    req3.field_E    = 1;
    func_8002E53C(&req3, "NMC");

    req4.x          = obj->baseX + 0x73 + xOff;
    req4.y          = obj->baseY + y;
    req4.otIndex    = (s16)obj->drawOrder + 1;
    req4.field_8    = 0x606060;
    req4.glyphTable = 5;
    req4.centerMode = 2;
    req4.field_E    = 1;
    func_8002E53C(&req4, "SCORE");

    req5.x          = obj->baseX + 0x96 + xOff;
    req5.y          = obj->baseY + y;
    req5.otIndex    = (s16)obj->drawOrder + 1;
    req5.field_8    = 0x606060;
    req5.glyphTable = 5;
    req5.centerMode = 2;
    req5.field_E    = 1;
    func_8002E53C(&req5, "KILL");

    req6.x          = obj->baseX - xOff;
    req6.y          = obj->baseY + y;
    req6.otIndex    = (s16)obj->drawOrder + 1;
    req6.field_8    = 0x606060;
    req6.glyphTable = 5;
    req6.centerMode = 2;
    req6.field_E    = 1;
    func_8002E53C(&req6, "TOTAL");

    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, "Result");

    if (task->state == 0) {
        if (gGameSession->field_126 == 1) {
            Ui_SetState4((Task*)obj, obj->owner);
            obj->field_2E = 6;
            task->state   = 0x100;
            return;
        }
        gGameSession->field_126 = 1;
        Ui_UpdateLayoutSize((UiPanel*)obj, 0, (rows * 0xB) + 0x21);
        obj->field_E = -((s16)obj->field_12 / 2);
        task->state  = task->state + 1;
    }

    status = obj->status;
    if ((status == 1) && (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
        bonus = func_mist_shooting_gallery_80184470(total);
        state = task->state;
        if (state == status) {
            if (bonus > 0) {
                flag = work->difficulty + 0x125;
                if (GameFlag_GetNibble(flag) == 0) {
                    if (func_mist_shooting_gallery_80184970(bonus) == state) {
                        GameFlag_SetNibble(flag, 2);
                    } else {
                        GameFlag_SetNibble(flag, 1);
                    }
                    Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018501C, total, 1, 1, obj);
                    obj->status = 0;
                } else {
                    obj->field_2E = 6;
                }
            } else {
                obj->field_2E = 6;
            }
        } else {
            obj->field_2E = 6;
        }
    }

    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        result   = childObj->field_2E;
        if (result == 6) {
            obj->field_2E = result;
        }
    }
}

void func_mist_shooting_gallery_8017E854(Task* task)
{
    u8            buf[0x20];
    TextDrawReq   req1;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    UiObject*     obj;
    s32           score;
    s32           bonus;
    s32           total;
    s32           xOff;
    s32           top;
    s32           y;
    s32           color;
    PlayerStatus* cfg;

    obj   = task->spawnArg2;
    score = task->spawnArg1;

    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, "BONUS");
    if (task->state == 0) {
        bonus = func_mist_shooting_gallery_80184470(score);
        cfg   = &Player_Status;
        if (bonus > 0) {
            total   = cfg->bp + bonus;
            cfg->bp = total;
            if (total > 999999) {
                cfg->bp = 999999;
            }
        }
        task->state = task->state + 1;
    }

    color = 0x606060;
    xOff  = (s16)obj->field_1C + 2;
    top   = (s16)obj->field_18;
    y     = top + 0xB;

    req1.x          = obj->baseX + xOff;
    req1.y          = (s16)(obj->baseY - 2) + y;
    req1.otIndex    = (s16)obj->drawOrder + 1;
    req1.field_8    = color;
    req1.glyphTable = 5;
    req1.centerMode = 0;
    req1.field_E    = 1;
    func_8002E53C(&req1, D_mist_shooting_gallery_8017D65C);

    req2.x          = obj->baseX - xOff;
    req2.y          = obj->baseY + y;
    req2.otIndex    = (s16)obj->drawOrder + 1;
    req2.field_8    = color;
    req2.glyphTable = 0;
    req2.centerMode = 2;
    req2.field_E    = 3;
    func_8002E53C(&req2, Text_ItoaSigned(buf, score));

    Ui_DrawHBar((UiPanel*)obj, xOff, -xOff, top + 0x1B);

    y               = top + 0x25;
    req3.x          = obj->baseX + xOff;
    req3.y          = (s16)(obj->baseY - 2) + y;
    req3.otIndex    = (s16)obj->drawOrder + 1;
    req3.field_8    = color;
    req3.glyphTable = 5;
    req3.centerMode = 0;
    req3.field_E    = 1;
    func_8002E53C(&req3, "BONUS BP");

    req4.x          = obj->baseX - xOff;
    req4.y          = obj->baseY + y;
    req4.otIndex    = (s16)obj->drawOrder + 1;
    req4.field_8    = 0x37A78;
    req4.glyphTable = 0;
    req4.centerMode = 2;
    req4.field_E    = 3;
    func_8002E53C(&req4, Text_ItoaSigned(buf, func_mist_shooting_gallery_80184470(score)));

    if ((obj->status == 1) && (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0)) {
        obj->field_2E = 6;
    }
}
INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6A0);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6AC);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6B8);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6C8);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6D8);

void func_mist_shooting_gallery_8017EAE0(Task* task)
{
    UiObject* obj  = task->spawnArg2;
    UiList*   list = &D_mist_shooting_gallery_8018503C;

    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, "SELECT");
    if (task->state == 0) {
        if (D_80072A92 == 0) {
            list->field_4 = 2;
            list->field_5 = 2;
        } else if (D_80072A92 < 2) {
            list->field_4 = 3;
            list->field_5 = 3;
        } else {
            list->field_4 = 4;
            list->field_5 = 4;
        }
        if (D_8007218B == 1) {
            list->field_4 = 4;
            list->field_5 = 4;
        }
        list->field_10 = 0;
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        list->field_A = 1;
        Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018507C[0], 0, 0, 1, obj);
        Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018507C[1], 0, 0, 1, obj);
        Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018507C[2], 0, 0, 1, obj);
        task->state = task->state + 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if ((obj->status == 1) && (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0)) {
        SndEvt_EnqueueType6(3, 0, 0);
        obj->field_2E = 6;
    }
}
void func_mist_shooting_gallery_8017EC58(Task* task)
{
    u8          buf[0x20];
    TextDrawReq req1;
    TextDrawReq req2;
    TextDrawReq req3;
    TextDrawReq req4;
    UiObject*   obj;
    s32         val;
    s32         q;
    s32         color;
    s32         rawExp;
    s32         rawBp;
    s32         xOff;
    s16         top;
    s32         y;

    obj = task->spawnArg2;
    Ui_DrawTitle((UiPanel*)obj, "STATUS");
    obj->field_2E = 0;
    if (task->state == 0) {
        Ui_UpdateLayoutSize((UiPanel*)obj, 0, Ui_Scale15(4));
        task->state = task->state + 1;
    }

    color = 0x606060;
    top   = (s16)obj->field_18;
    y     = top + 0xF;
    val   = Gp_StatRows[Mc_SaveData.gameMode].base.word;
    xOff  = (s16)obj->field_1C + 6;
    if (val < 100) {
        color = 0xD287F;
    }

    req1.x          = obj->baseX + xOff;
    req1.y          = (s16)(obj->baseY - 8) + y;
    req1.otIndex    = (s16)obj->drawOrder + 1;
    req1.field_8    = 0x606060;
    req1.glyphTable = 5;
    req1.centerMode = 0;
    req1.field_E    = 1;
    func_8002E53C(&req1, "HP");
    Text_DrawPrompt(obj, -xOff, y, Text_ItoaSigned(buf, val), color, 3, 2);

    y     = top + 0x1E;
    val   = Gp_StatRows[Mc_SaveData.gameMode].field_4;
    color = 0x606060;
    if (val < 30) {
        color = 0xD287F;
    }

    req2.x          = obj->baseX + xOff;
    req2.y          = (s16)(obj->baseY - 8) + y;
    req2.otIndex    = (s16)obj->drawOrder + 1;
    req2.field_8    = 0x606060;
    req2.glyphTable = 5;
    req2.centerMode = 0;
    req2.field_E    = 1;
    func_8002E53C(&req2, "MP");
    Text_DrawPrompt(obj, -xOff, y, Text_ItoaSigned(buf, val), color, 3, 2);

    y      = top + 0x2D;
    rawExp = D_mist_shooting_gallery_8018E0BC;
    switch (Mc_SaveData.gameMode) {
        case 3:
            val = 0;
            break;
        case 2:
            q = rawExp / 100;
            goto clamp_exp;
        case 1:
            q = rawExp / 20;
            goto clamp_exp;
        default:
            q = rawExp / 10;
        clamp_exp:
            if (q > 999999) {
                q = 999999;
            }
            val = q;
            break;
    }

    req3.x          = obj->baseX + xOff;
    req3.y          = (s16)(obj->baseY - 8) + y;
    req3.otIndex    = (s16)obj->drawOrder + 1;
    req3.field_8    = 0x606060;
    req3.glyphTable = 5;
    req3.centerMode = 0;
    req3.field_E    = 1;
    func_8002E53C(&req3, "EXP");
    Text_DrawPrompt(obj, -xOff, y, Text_ItoaSigned(buf, val), 0x606060, 3, 2);

    y    += 0xF;
    rawBp = D_mist_shooting_gallery_8018E0C0;
    switch (D_80072177) {
        case 3:
            val = 0;
            break;
        case 2:
            q = rawBp / 100;
            goto clamp_bp;
        case 1:
            q = rawBp / 20;
            goto clamp_bp;
        default:
            q = rawBp / 10;
        clamp_bp:
            if (q > 999999) {
                q = 999999;
            }
            val = q;
            break;
    }

    req4.x          = obj->baseX + xOff;
    req4.y          = (s16)(obj->baseY - 8) + y;
    req4.otIndex    = (s16)obj->drawOrder + 1;
    req4.field_8    = 0x606060;
    req4.glyphTable = 5;
    req4.centerMode = 0;
    req4.field_E    = 1;
    func_8002E53C(&req4, "BP");
    Text_DrawPrompt(obj, -xOff, y, Text_ItoaSigned(buf, val), 0x606060, 3, 2);
}
INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D708);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D718);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D720);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D728);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D730);

void func_mist_shooting_gallery_8017F128(Task* task)
{
    UiObject*                  obj           = task->spawnArg2;
    MistShootingGalleryRatings missionLevels = { {
        { 2, D_mist_shooting_gallery_8017D718 },
        { 3, D_mist_shooting_gallery_8017D720 },
        { 4, D_mist_shooting_gallery_8017D728 },
        { 5, D_mist_shooting_gallery_8017D730 },
    } };
    MistShootingGalleryRatings conditions;
    MistShootingGalleryRatings enemyLevels;
    MistShootingGalleryRatings supplyLevels;
    MistShootingGalleryGauges  gauges;
    TextDrawReq                label0;
    TextDrawReq                value0;
    TextDrawReq                label1;
    TextDrawReq                value1;
    TextDrawReq                label2;
    TextDrawReq                value2;
    TextDrawReq                label3;
    TextDrawReq                value3;
    MistShootingGalleryRating* rating;
    s32                        col;
    s32                        row;
    s32                        x;
    s32                        y;

    conditions   = D_mist_shooting_gallery_8017D778;
    enemyLevels  = D_mist_shooting_gallery_8017D7AC;
    supplyLevels = D_mist_shooting_gallery_8017D7E8;
    gauges       = D_mist_shooting_gallery_8017D808;
    Ui_DrawTitle((UiPanel*)obj, D_mist_shooting_gallery_8017D820);

    col               = obj->field_1C;
    obj->field_2E     = 0;
    x                 = col + 0xB;
    row               = (s16)obj->field_18;
    label0.x          = obj->baseX + x;
    y                 = row + 0xB;
    label0.y          = (s16)(obj->baseY - 6) + y;
    label0.otIndex    = (s16)obj->drawOrder + 1;
    rating            = &missionLevels.entries[Mc_SaveData.gameMode];
    label0.field_8    = 0x606060;
    label0.glyphTable = 5;
    label0.centerMode = 0;
    label0.field_E    = 1;
    func_8002E53C(&label0, D_mist_shooting_gallery_8017D828);

    value0.x          = obj->baseX + 0x41;
    value0.y          = (s16)(obj->baseY - 3) + y;
    value0.otIndex    = (s16)obj->drawOrder + 1;
    value0.field_8    = 0x606060;
    value0.glyphTable = 0;
    value0.centerMode = 2;
    value0.field_E    = 3;
    func_8002E53C(&value0, rating->label);
    Text_DrawPrompt(obj, 0x46, y, gauges.bars[rating->gauge], 0x606060, 3, 0);
    Ui_DrawHBar((UiPanel*)obj, col + 6, -x + 5, row + 0xD);

    y                 = row + 0x1E;
    label1.x          = obj->baseX + x;
    label1.y          = (s16)(obj->baseY - 6) + y;
    label1.otIndex    = (s16)obj->drawOrder + 1;
    rating            = &conditions.entries[Mc_SaveData.gameMode];
    label1.field_8    = 0x606060;
    label1.glyphTable = 5;
    label1.centerMode = 0;
    label1.field_E    = 1;
    func_8002E53C(&label1, D_mist_shooting_gallery_8017D838);

    value1.x          = obj->baseX + 0x41;
    value1.y          = (s16)(obj->baseY - 3) + y;
    value1.otIndex    = (s16)obj->drawOrder + 1;
    value1.field_8    = 0x606060;
    value1.glyphTable = 0;
    value1.centerMode = 2;
    value1.field_E    = 3;
    func_8002E53C(&value1, rating->label);
    Text_DrawPrompt(obj, 0x46, y, gauges.bars[rating->gauge], 0x606060, 3, 0);

    y                 = row + 0x2D;
    label2.x          = obj->baseX + x;
    label2.y          = (s16)(obj->baseY - 6) + y;
    label2.otIndex    = (s16)obj->drawOrder + 1;
    rating            = &enemyLevels.entries[Mc_SaveData.gameMode];
    label2.field_8    = 0x606060;
    label2.glyphTable = 5;
    label2.centerMode = 0;
    label2.field_E    = 1;
    func_8002E53C(&label2, D_mist_shooting_gallery_8017D844);

    value2.x          = obj->baseX + 0x41;
    value2.y          = (s16)(obj->baseY - 3) + y;
    value2.otIndex    = (s16)obj->drawOrder + 1;
    value2.field_8    = 0x606060;
    value2.glyphTable = 0;
    value2.centerMode = 2;
    value2.field_E    = 3;
    func_8002E53C(&value2, rating->label);
    Text_DrawPrompt(obj, 0x46, y, gauges.bars[rating->gauge], 0x606060, 3, 0);

    y                 = row + 0x3C;
    label3.x          = obj->baseX + x;
    label3.y          = (s16)(obj->baseY - 6) + y;
    label3.otIndex    = (s16)obj->drawOrder + 1;
    rating            = &supplyLevels.entries[Mc_SaveData.gameMode];
    label3.field_8    = 0x606060;
    label3.glyphTable = 5;
    label3.centerMode = 0;
    label3.field_E    = 1;
    func_8002E53C(&label3, D_mist_shooting_gallery_8017D850);

    value3.x          = obj->baseX + 0x41;
    value3.y          = (s16)(obj->baseY - 3) + y;
    value3.otIndex    = (s16)obj->drawOrder + 1;
    value3.field_8    = 0x606060;
    value3.glyphTable = 0;
    value3.centerMode = 2;
    value3.field_E    = 3;
    func_8002E53C(&value3, rating->label);
    Text_DrawPrompt(obj, 0x46, y, gauges.bars[rating->gauge], 0x606060, 3, 0);
}
/// Task handler for the gallery's closing sequence. State 0 spawns the results
/// panel and stashes the player's `Player_Status` BP (`field_8`) and experience
/// (`field_C`) totals in `D_mist_shooting_gallery_8018E0BC` / `_8018E0C0`.
/// State 1 waits for the panel to confirm (`field_2E == 6`), then writes both
/// totals back scaled down by the bonus mode - the same divisor table as
/// `func_mist_shooting_gallery_8017FA38`, clamped to 999999. Once the kill
/// countdown runs out the task exits and the stage is flagged as ended.
void func_mist_shooting_gallery_8017F6C8(Task* task)
{
    UiObject*     obj;
    PlayerStatus* cfg = &Player_Status;
    s32           savedBp;
    s32           savedExp;
    s32           bp;
    s32           exp;

    if (task->state == 0) {
        obj = Ui_SpawnFromDesc(&D_mist_shooting_gallery_80185060, 0, 1, 1, NULL);
        if (obj != NULL) {
            D_mist_shooting_gallery_8018E0C0 = cfg->bp;
            D_mist_shooting_gallery_8018E0BC = cfg->exp;
            GameMain_SetFrameTiming(0);
            task->spawnArg2 = obj;
            task->state     = task->state + 1;
        }
    } else if (task->state == 1) {
        obj = task->spawnArg2;
        if (obj->field_2E == 6) {
            task->killCountdown = 0xA;
            Ui_TeardownTree(obj, obj->owner);
            Gp_RecalcMaxHp();
            Gp_RecalcMaxMp();

            savedBp = D_mist_shooting_gallery_8018E0BC;
            switch (D_80072177) {
                case 3:
                    bp = 0;
                    goto store_bp;
                case 2:
                    bp = savedBp / 100;
                    break;
                case 1:
                    bp = savedBp / 20;
                    break;
                default:
                    bp = savedBp / 10;
                    break;
            }
            if (bp > 999999) {
                bp = 999999;
            }
        store_bp:
            cfg->exp = bp;

            savedExp = D_mist_shooting_gallery_8018E0C0;
            switch (D_80072177) {
                case 3:
                    exp = 0;
                    goto store_exp;
                case 2:
                    exp = savedExp / 100;
                    break;
                case 1:
                    exp = savedExp / 20;
                    break;
                default:
                    exp = savedExp / 10;
                    break;
            }
            if (exp > 999999) {
                exp = 999999;
            }
        store_exp:
            cfg->bp = exp;
            Gp_FillHpMp();
            task->state = task->state + 1;
        }
    } else {
        task->killCountdown = task->killCountdown - 1;
        if (task->killCountdown < 0) {
            Task_CallExit(task);
            GameMain_SetFrameTiming(1);
            Wip_UiHolder = NULL;
            Stage_SetEndingFlag();
        }
    }
}
s32 func_mist_shooting_gallery_8017F95C(void)
{
    Display_InitModeObj(&D_mist_shooting_gallery_80184F8C, 0x44, 0, 0);
    return 1;
}

void func_mist_shooting_gallery_8017F98C(DialogPrompt* arg0, UiObject* arg1)
{
    MistShootingGalleryModeTexts texts;
    s32                          one;

    texts = D_mist_shooting_gallery_8017D6D8;
    one   = 1;
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A - 1, texts.text[arg0->field_8], arg0->field_1C, one, 0);
    if (arg0->field_C == one) {
        D_80072177 = (u8)arg0->field_8;
    }
}
s32 func_mist_shooting_gallery_8017FA38(s32 score)
{
    s32 value;

    switch (D_80072177) {
        case 3:
            return 0;
        case 2:
            value = score / 100;
            break;
        case 1:
            value = score / 20;
            break;
        default:
            value = score / 10;
            break;
    }
    if (value > 999999) {
        value = 999999;
    }
    return value;
}
void func_mist_shooting_gallery_8017FAE8(Task* task)
{
    UiObject*                    obj   = task->spawnArg2;
    MistShootingGalleryModeTexts texts = D_mist_shooting_gallery_8017D708;

    obj->field_2E = 0;
    if (task->state == 0) {
        Ui_UpdateLayoutSize((UiPanel*)obj, 0, Ui_Scale15(3) + 1);
        obj->field_E = 0x68 - obj->field_12;
        task->state  = task->state + 1;
    }
    Text_DrawMultiLine(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, texts.text[D_80072177], 0x606060, 1, 0);
}
void func_mist_shooting_gallery_8017FBD8(void)
{
    if ((D_80072176 > 0) && (gGameSession->at4.loc.warp == 7)) {
        Display_InitModeObj(&D_mist_shooting_gallery_801850D0, 0, 0, 0);
    }
}
void func_mist_shooting_gallery_8017FC2C(Task* arg0)
{
    s32 var_a0;

    arg0->msgTable = D_mist_shooting_gallery_801850E8;
    Game_SetPtrSlot(arg0, 7);
    func_8014C5E0(0x340, 0, 2);
    if (GameFlag_GetNibble(0xED) != 0) {
        Gp_MsgSlot4Chain(1, 0);
        var_a0 = 1;
    } else {
        var_a0 = 0;
    }
    func_mist_shooting_gallery_801801E4(var_a0);
    if (D_8007218B == 7) {
        Task_SpawnFromTable(&D_mist_shooting_gallery_801856B8, 0, 0, 0);
    } else if (gGameSession->at4.loc.warp == 7) {
        Task_SpawnFromTable(&D_8014E13C, 0, 0, 0);
    }
    if ((gGameSession->at4.loc.warp == 6) && (GameFlag_GetNibble(0xED) != 0)) {
        Gp_RunCapCmd1(0x16);
    }
    gGameSession->flowFlags = 2;
    arg0->state             = arg0->state + 1;
}

void func_mist_shooting_gallery_8017FD40(Task* task)
{
    u8 temp_v1;

    if ((gGameSession->at4.loc.place == 1) && (gGameSession->eventState == 0)) {
        temp_v1 = gGameSession->at4.loc.view;
        if ((temp_v1 == 3) || (temp_v1 == 9) || (temp_v1 == 0x12)) {
            Gp_MsgSlot4Chain(1, 0);
        } else if (GameFlag_GetNibble(0xED) == 0) {
            Gp_MsgSlot4Chain(1, 1);
        }
    }
    func_8014A398();
}
void func_mist_shooting_gallery_8017FDD0(Task* arg0)
{
    s16 var_a0;

    switch (arg0->state) {
        case 0:
            Gp_CapFile = 0;
            if (arg0->spawnArg2 == (void*)3) {
                Gp_LoadCapFile(3);
                var_a0 = 0x2C0;
            } else {
                Gp_LoadCapFile(1);
                var_a0 = 0x300;
            }
            func_800E6D4C(var_a0, 0);
            Gp_RunCapCmd(arg0->spawnArg1, 0);
            goto block_inc;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
        block_inc:
            arg0->state += 1;
            return;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(arg0);
            break;
    }
}
INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D75C);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D764);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D770);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D778);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D798);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D7A0);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D7AC);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D7CC);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D7D4);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D7E0);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D7E8);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D808);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D820);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D828);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D838);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D844);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D850);

/// The room's handler for message 0x13F1: accepts it and does nothing.
s32 func_mist_shooting_gallery_8017FEB0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_mist_shooting_gallery_8017FEB8(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (*(u16*)src == 0x13 && src->field_5 == 0) {
        if (GameFlag_GetNibble(0x7A) != 0) {
            dst->field_3 += 2;
        }
    }
    if (*(u16*)src == 0x14) {
        if (dst->field_2 == 5 && func_8014AA54(src) == 2) {
            return 2;
        }
        if (src->field_5 == 0) {
            if (dst->field_2 == 6) {
                D_8007272D            = 2;
                D_80073BAE            = 4;
                gGameSession->hideHud = 1;
                Gp_ResetInventory();
            }
            if (dst->field_2 == 5) {
                D_8007272D            = 1;
                D_80073BAE            = 3;
                gGameSession->hideHud = 1;
                Gp_ClearInventory();
            }
        }
    }
    return 1;
}

s32 func_mist_shooting_gallery_80180000(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 >= 5) {
        if (arg2 >= 9) {
            if (arg2 < 0x23) {
                if (arg2 >= 0x21) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, 3);
                }
            }
        } else {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, 1);
        }
    }
    return 0;
}

s32 func_mist_shooting_gallery_8018008C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 1) && (D_8014D038 == 0)) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_8014E13C, 1, 1, 0);
        D_80114D08 = 0xA;
    }
    if ((arg2->field_2 == 2) && (GameFlag_GetNibble(0xED) == 0)) {
        func_8014AF0C();
    }
    if (arg2->field_2 == 3) {
        func_8014AB6C();
    }
    if ((arg2->field_2 == 4) && (GameFlag_GetNibble(0x106) == 0)) {
        func_800E3FAC(0xA2, 0x3B);
        GameFlag_SetNibble(0x106, 1);
        func_800E8634((s32)&D_80153274, 0, (s32)&D_80153D6C);
    }
    return 0;
}

/// The room task's threeGpGridParamsstate table, run from a stack copy by
/// `func_mist_shooting_gallery_8018018C`: the entry tick
/// `func_mist_shooting_gallery_8017FC2C`, the perGpGridParamsframe state
/// `func_mist_shooting_gallery_8017FD40`, then `taskKill`.
const TaskFuncTable3 D_mist_shooting_gallery_8017D860 = {
    { func_mist_shooting_gallery_8017FC2C, func_mist_shooting_gallery_8017FD40, taskKill },
};

/// The jukebox's track names ("1. Crazy King", ...), reached only through the
/// track lists in `.data`.
INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D86C);

/// The room task: copies the threeGpGridParamsstate table
/// `D_mist_shooting_gallery_8017D860` onto the stack and runs the entry for the
/// task's current state - the entry tick `func_mist_shooting_gallery_8017FC2C`,
/// the perGpGridParamsframe state `func_mist_shooting_gallery_8017FD40`, then `taskKill`.
void func_mist_shooting_gallery_8018018C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_shooting_gallery_8017D860;
    sp.funcs[task->state](task);
}

void func_mist_shooting_gallery_801801E4(s32 arg0)
{
    GpGridParams* dst = &D_mist_shooting_gallery_80189968;
    GpGridParams* src = &D_mist_shooting_gallery_80185198;
    SVECTOR       ofs;
    s32           i;

    for (i = 0; i < 3; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }
    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }
    if (arg0 == 0) {
        ofs.vx = 0;
        ofs.vy = 0;
    } else {
        ofs.vx = 0;
        ofs.vy = 0xBB8;
    }
    ofs.vz = 0;
    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx += ofs.vx;
        dst->field_8[i].vy += ofs.vy;
        dst->field_8[i].vz += ofs.vz;
    }
}

void func_mist_shooting_gallery_80180390(s32 arg0)
{
    GpGridParams* dst    = &D_mist_shooting_gallery_80189968;
    GpGridParams* src    = &D_mist_shooting_gallery_801851F8;
    GpGridFace*   dlinks = &D_mist_shooting_gallery_80189968.field_C[3];
    GpGridFace*   slinks = D_mist_shooting_gallery_801851F8.field_C;
    SVECTOR       ofs;
    s32           i;
    s32           j;

    for (i = 0; i < 1; i++) {
        dst->field_4[i + 3].vx = src->field_4[i].vx;
        dst->field_4[i + 3].vy = src->field_4[i].vy;
        dst->field_4[i + 3].vz = src->field_4[i].vz;
        for (j = 0; j < 4; j++) {
            dlinks->verts[j] = slinks->verts[j] + 8;
        }
        dlinks->field_8 = slinks->field_8 + 3;
        dlinks->field_A = slinks->field_A;
        dlinks++;
        slinks++;
    }
    for (i = 0; i < 4; i++) {
        dst->field_8[i + 8].vx = src->field_8[i].vx;
        dst->field_8[i + 8].vy = src->field_8[i].vy;
        dst->field_8[i + 8].vz = src->field_8[i].vz;
    }
    if (arg0 == 0) {
        ofs.vx = 0;
        ofs.vy = 0;
    } else {
        ofs.vx = 0;
        ofs.vy = 0xFA0;
    }
    ofs.vz = 0;
    for (i = 0; i < 8; i++) {
        dst->field_8[i + 8].vx += ofs.vx;
        dst->field_8[i + 8].vy += ofs.vy;
        dst->field_8[i + 8].vz += ofs.vz;
    }
}

/// The jukebox's ten track lists: one per game mode, with list 4 standing in
/// before the first clear, and the second five used outside the debug attach
/// room.
const RoomsShared8018055cMenu D_mist_shooting_gallery_8017DADC = {
    {
        D_mist_shooting_gallery_8018521C,
        D_mist_shooting_gallery_80185234,
        D_mist_shooting_gallery_8018524C,
        D_mist_shooting_gallery_80185264,
        D_mist_shooting_gallery_8018527C,
        D_mist_shooting_gallery_80185294,
        D_mist_shooting_gallery_801852B4,
        D_mist_shooting_gallery_801852D4,
        D_mist_shooting_gallery_801852F4,
        D_mist_shooting_gallery_80185314,
    },
};

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017DB04);

/// Row callback of the jukebox list: draws the row's track name, and on
/// confirm, when the row is not the one already chosen, plays the select
/// sound and, when the track differs from the one playing, fades the music
/// out and hands the track id to the menu task to load.
void func_mist_shooting_gallery_8018055C(DialogPrompt* prompt, UiObject* obj)
{
    RoomsShared8018055cMenu    menu;
    RoomsShared8018055cCourse* course;
    s32                        row;
    s32                        list;
    s32                        mode;

    row  = prompt->field_8;
    menu = D_mist_shooting_gallery_8017DADC;

    list = 4;
    if (Mc_SaveData.clearCount != 0) {
        list = Mc_SaveData.gameMode;
    }
    if (Gp_IsDebugAttachRoom() == 0) {
        list += 5;
    }

    course              = &menu.lists[list][row];
    menu.req.x          = obj->baseX + (u16)prompt->field_18;
    menu.req.y          = (prompt->field_1A - 3) + obj->baseY;
    menu.req.otIndex    = (s16)obj->drawOrder + 1;
    menu.req.field_8    = prompt->field_1C;
    menu.req.glyphTable = 4;
    menu.req.field_E    = 1;
    menu.req.centerMode = 0;
    func_8002E53C(&menu.req, course->name);

    mode = prompt->field_C;
    if (mode == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            if (obj->owner->spawnArg1 != prompt->field_8) {
                SndEvt_EnqueueType6(0x16, 0, 0);
                if (obj->owner->status != course->id) {
                    SndEvt_EnqueueType2(0, 0x3C);
                    obj->owner->state  = mode;
                    obj->owner->status = course->id;
                    CdCmd_DropPending();
                }
                obj->owner->spawnArg1 = prompt->field_8;
            }
        }
    }
}

/// The jukebox menu task, the update routine of the panel
/// `D_mist_shooting_gallery_8018535C` builds. Draws the title and, on its first
/// tick, lays out the track list (four rows, three in the debug attach room).
/// While a chosen track is pending it waits for the MIDI player to go idle,
/// queues the track's CD load, then starts it once the CD is idle and records
/// it as the current track. The menu or cancel button plays the back sound and
/// closes the panel.
void func_mist_shooting_gallery_80180728(Task* task)
{
    u8        param1[8];
    u8        param2[8];
    UiObject* obj;
    UiList*   menu;
    u8        flags;
    s32       sent;
    s32       state;
    u8        ready;

    obj  = task->spawnArg2;
    menu = &D_mist_shooting_gallery_80185338;

    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_mist_shooting_gallery_8017DB04);
    if (task->state == 0) {
        task->spawnArg1 = -1;
        if (Gp_IsDebugAttachRoom() == 0) {
            menu->field_4 = 4;
        } else {
            menu->field_4 = 3;
        }
        if (menu->field_4 >= 0xB) {
            menu->field_5 = 0xA;
        } else {
            menu->field_5 = menu->field_4;
        }
        menu->field_10 = 0;
        menu->field_9  = 0;
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        menu->field_A = 1;
        Ui_SetListScrollFlag(menu, 1);
        obj->field_C = -((s16)obj->field_10 / 2);
        obj->field_E = -((s16)obj->field_12 / 2);
        if (Gp_IsDebugAttachRoom() == 0) {
            task->status = 0xFF;
        } else {
            task->status = 0xFE;
        }
        task->state += 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    flags = task->status;
    if (flags < 0xF1) {
        state = task->state;
        if (state == 1) {
            if (Midi_IsBusy(0) == 0) {
                param1[3] = 0;
                param1[2] = 4;
                param1[0] = flags;
                param2[0] = state;
                param2[3] = 0;
                param2[2] = 0;
                param2[1] = 0;
                CdCmd_Enqueue(0x21, param1, param2);
                sent = 1;
            } else {
                sent = 0;
            }
            if (sent == 1) {
                task->state += 1;
            }
        } else {
            if (CdCmd_IsIdle() & 0xFFFF) {
                SndEvt_EnqueueType1(flags, 0);
                SndEvt_EnqueueType5(flags, (u8)D_8007A396);
                ready          = 1;
                gStageRoomSong = flags;
            } else {
                ready = 0;
            }
            if (ready == 1) {
                task->state  = 1;
                task->status = 0xFF;
                if (Gp_IsDebugAttachRoom() == 0) {
                    gGameSession->flowFlags |= 3;
                }
                if (obj->status != 1) {
                    obj->field_2E = 6;
                }
            }
        }
    }
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskMenu | Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            if (task->status != 0xFE) {
                if (task->status == 0xFF) {
                    obj->field_2E = 6;
                } else {
                    Ui_SetState4((Task*)obj, obj->owner);
                    obj->status = 0;
                }
            }
        }
    }
}

/// Runs the jukebox panel over the room: takes the prim buffer and stops the
/// frame timer while the panel is up, waits for the panel to report closed,
/// then after ten more ticks gives both back, kills itself and ends the stage.
void func_mist_shooting_gallery_80180A00(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018535C, task->spawnArg1, 1, 1, NULL);
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

s32 func_mist_shooting_gallery_80180B34(void)
{
    Display_InitModeObj(&D_mist_shooting_gallery_80185378, 0, 0, 0);
    return 1;
}

void func_mist_shooting_gallery_80180B64(Task* arg0)
{
    u8 param1[8];
    u8 param2[8];

    switch (arg0->state) {
        case 0:
            param1[3] = 0;
            param1[2] = 0;
            param2[0] = 0;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            switch (D_80072177) {
                case 0:
                    param1[0] = 0x29;
                    break;
                case 1:
                    param1[0] = 0x2A;
                    break;
                case 2:
                    param1[0] = 0x2B;
                    break;
                case 3:
                    param1[0] = 0x2C;
                    break;
            }
            CdCmd_Enqueue(0x21, param1, param2);
            arg0->state++;
            return;

        case 1:
            if (CdCmd_IsIdle() & 0xFFFF) {
                D_80071068[0]       = 1;
                arg0->killCountdown = 0;
                arg0->state++;
                return;
            }
            return;

        case 2: {
            TILE*     p;
            DR_TPAGE* dr;
            u8        color;

            p              = (TILE*)gGpuPrimCursor;
            color          = ~(u8)arg0->killCountdown;
            gGpuPrimCursor = p + 1;
            setlen(p, 3);
            setcode(p, 0x62);
            p->r0 = color;
            p->g0 = color;
            p->b0 = color;
            p->x0 = -0xA0;
            p->y0 = -0x78;
            p->w  = 0x140;
            p->h  = 0xF0;

            addPrim(gGpuCurrentOt, p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt, dr);

            arg0->killCountdown += 8;
            if (arg0->killCountdown >= 0x11) {
                SetDispMask(1);
            }
            if (arg0->killCountdown < 0x100) {
                return;
            }
            arg0->killCountdown = 0;
            arg0->state++;
            return;
        }

        case 3:
            arg0->killCountdown += 1;
            if (arg0->killCountdown < 0x97 && Pad_CheckFlag800() == 0) {
                return;
            }
            arg0->killCountdown = 0;
            arg0->state++;
            return;

        case 4: {
            TILE*     p;
            DR_TPAGE* dr;
            u8        color;

            p              = (TILE*)gGpuPrimCursor;
            color          = (u8)arg0->killCountdown;
            gGpuPrimCursor = p + 1;
            setlen(p, 3);
            setcode(p, 0x62);
            p->r0 = color;
            p->g0 = color;
            p->b0 = color;
            p->x0 = -0xA0;
            p->y0 = -0x78;
            p->w  = 0x140;
            p->h  = 0xF0;

            addPrim(gGpuCurrentOt, p);
            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt, dr);

            arg0->killCountdown += 8;
            if (arg0->killCountdown < 0x100) {
                return;
            }
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(0);
            arg0->state++;
            return;
        }

        case 5:
            D_80071068[0] = 0;
            taskKill(arg0);
            Display_ResetHeapWrapper();
            return;

        default:
            return;
    }
}

void func_mist_shooting_gallery_80180F2C(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slot         = Stream_FindSlot(key.raw.data, 0, 0);
    slotParam[0] = slot;
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    gDisplayState.at100.flags.imageSource = 1;
    Mem_Set(Fs_ImgBuffers, 0, 0x25800);
    taskKill(task);
    Display_ResetHeapWrapper();
}

void func_mist_shooting_gallery_801810D8(Task* task)
{
    switch (task->state) {
        case 0:
            SetDispMask(0);
            if (D_80072176 == 0) {
                task->state = 2;
                return;
            }
            Display_SpawnWithOt(&D_mist_shooting_gallery_80185384, 2, 0, 0);
            D_8007106B = 0;
            Gp_SpawnViewTasks();
        case 1:
            task->state = task->state + 1;
            return;
        case 2:
            Display_SpawnWithOt(&D_mist_shooting_gallery_80185384, 1, 0, 0);
            D_8007106B = 1;
            Gp_SpawnViewTasks();
            taskKill(task);
            return;
    }
}

/// Publishes one of the room's two data banks as the active one: bank 0 for a
/// zero argument, bank 1 otherwise.
void func_mist_shooting_gallery_801811C0(s16 arg0)
{
    if (arg0 == 0) {
        D_mist_shooting_gallery_801853C0 = D_mist_shooting_gallery_8018D1B4;
        return;
    }
    D_mist_shooting_gallery_801853C0 = D_mist_shooting_gallery_8018DF38;
}

void func_mist_shooting_gallery_801811EC(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_80185550[0], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_80185550[8], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_80185550[10], 0x200, 0x222);
            break;
        case 3:
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_80185570[0], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_80185570[2], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_80185570[10], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_80185570[12], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_80185570[14], 0x200, 0x222);
            break;
        case 7:
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_801855C0[0], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_801855C0[2], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_801855C0[4], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_801855C0[6], 0x200, 0x222);
            break;
        case 8:
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_80185610[0], 0x200, 0x222);
            break;
        case 9:
        case 18:
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_801855F0[0], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_801855F0[2], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_801855F0[6], 0x200, 0x222);
            func_mist_shooting_gallery_80181480(&D_mist_shooting_gallery_801855F0[8], 0x200, 0x222);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_801855F0[16], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
        case 10:
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185678[0], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185678[2], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185678[4], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185678[6], 0x300, 0x111);
            break;
        case 11:
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185680[0], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185680[1], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185680[3], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185680[4], 0x300, 0x111);
            break;
        case 12:
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185690[0], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185690[1], 0x300, 0x111);
            break;
        case 13:
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185688[0], 0x300, 0x111);
            break;
        case 14:
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185670[0], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185670[1], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185670[3], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185670[5], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_80185670[7], 0x300, 0x111);
            func_mist_shooting_gallery_80181CC4(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
    }
}

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`,
/// projected through `Gfx_ViewWorldMtx`; nothing is drawn unless both project.
/// Each end is a halfGpGridParamsdisc of screen radius `arg1 * 64 / otz` and the two are
/// joined by a band, built from gouraud quads lit at the centre line and black
/// at the rim, in two 0x400 steps around the angle between the projected
/// points. `arg2` is the colour as three 4GpGridParamsbit channels (0xRGB), brightened
/// slightly on odd frames.
void func_mist_shooting_gallery_80181480(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      sum;
    u8                       r;
    u8                       g;
    u8                       b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a glowing disc around the point `arg0`, projected through
/// `Gfx_ViewWorldMtx`, unless the projection flags an error: four gouraud
/// wedges lit at the projected centre and black at the rim, of screen radius
/// `arg1 * 64 / otz`. `arg2` is the colour as three 4GpGridParamsbit channels (0xRGB),
/// brightened slightly on odd frames.
void func_mist_shooting_gallery_80181CC4(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw31Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw31Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw31Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw31Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw31Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
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
    SCRATCH_POP_BYTES(0x10);
}
