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

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017E854);
INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6A0);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6AC);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6B8);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6C8);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D6D8);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017EAE0);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017EC58);
INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D708);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D718);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D720);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D728);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", D_mist_shooting_gallery_8017D730);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017F128);
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

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017F98C);
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
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery", func_mist_shooting_gallery_8017FC2C);
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
