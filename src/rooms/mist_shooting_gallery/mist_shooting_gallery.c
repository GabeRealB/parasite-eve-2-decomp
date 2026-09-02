#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
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

#include "rooms/mist_shooting_gallery.h"

extern UiObjectDesc D_mist_shooting_gallery_80185060;
extern s32          D_mist_shooting_gallery_8018E0BC;
extern s32          D_mist_shooting_gallery_8018E0C0;

extern TaskDesc D_mist_shooting_gallery_80184F8C;
extern TaskDesc D_mist_shooting_gallery_801850D0;
extern s8       D_80072176;
extern s8       D_80072177;

extern u8 D_mist_shooting_gallery_8017D65C[]; // "TOTAL SCORE"

extern s32 func_mist_shooting_gallery_80184470(s32);

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

extern void     func_8014A398(void);
extern void     func_8014C5E0(s32, s32, s32);
extern void     func_mist_shooting_gallery_801801E4(s32);
extern s8       D_8007218B;
extern s8       D_80072A92;
extern TaskDesc D_8014E13C;

extern GpItemScan D_80072724;
extern u8         Gp_DebugAttachLevels[];

extern s8           D_80073BA9;
extern UiObjectDesc D_8010EFA0;

/// The ten weapons the gallery's weapon picker offers, in row order. Rows whose
/// item is not unlocked yet (`func_800B7420` returns 0) are skipped, so
/// `DialogPrompt::field_8` counts *drawn* rows, not table slots.
extern s16 D_mist_shooting_gallery_80184F34[];

extern UiList D_mist_shooting_gallery_80184F4C;

extern UiList       D_mist_shooting_gallery_8018503C;
extern UiObjectDesc D_mist_shooting_gallery_8018507C[];
extern GpMsgEntry   D_mist_shooting_gallery_801850E8[];
extern TaskDesc     D_mist_shooting_gallery_801856B8;

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017DCAC);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017DE7C);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017E090);
INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D65C);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017E234);

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
    WipSysConfig* cfg;

    obj   = task->spawnArg2;
    score = task->spawnArg1;

    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, "BONUS");
    if (task->state == 0) {
        bonus = func_mist_shooting_gallery_80184470(score);
        cfg   = &Wip_SysConfig;
        if (bonus > 0) {
            total        = cfg->field_C + bonus;
            cfg->field_C = total;
            if (total > 999999) {
                cfg->field_C = 999999;
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
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017EC58);
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
    rating            = &missionLevels.entries[Mc_SaveData.field_F];
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
    rating            = &conditions.entries[Mc_SaveData.field_F];
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
    rating            = &enemyLevels.entries[Mc_SaveData.field_F];
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
    rating            = &supplyLevels.entries[Mc_SaveData.field_F];
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
/// panel and stashes the player's `Wip_SysConfig` BP (`field_8`) and experience
/// (`field_C`) totals in `D_mist_shooting_gallery_8018E0BC` / `_8018E0C0`.
/// State 1 waits for the panel to confirm (`field_2E == 6`), then writes both
/// totals back scaled down by the bonus mode - the same divisor table as
/// `func_mist_shooting_gallery_8017FA38`, clamped to 999999. Once the kill
/// countdown runs out the task exits and the stage is flagged as ended.
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017F6C8);
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
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017FA38);
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
    if ((D_80072176 > 0) && (Game_Session->field_8 == 7)) {
        Display_InitModeObj(&D_mist_shooting_gallery_801850D0, 0, 0, 0);
    }
}
void func_mist_shooting_gallery_8017FC2C(Task* arg0)
{
    s32 var_a0;

    arg0->field_24 = D_mist_shooting_gallery_801850E8;
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
    } else if (Game_Session->field_8 == 7) {
        Task_SpawnFromTable(&D_8014E13C, 0, 0, 0);
    }
    if ((Game_Session->field_8 == 6) && (GameFlag_GetNibble(0xED) != 0)) {
        Gp_RunCapCmd1(0x16);
    }
    Game_Session->field_69 = 2;
    arg0->state            = arg0->state + 1;
}
void func_mist_shooting_gallery_8017FD40(void)
{
    u8 temp_v1;

    if ((Game_Session->field_9 == 1) && (Game_Session->field_1 == 0)) {
        temp_v1 = Game_Session->field_4;
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
            Task_Kill(arg0);
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

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D860);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017DADC);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017DB04);
