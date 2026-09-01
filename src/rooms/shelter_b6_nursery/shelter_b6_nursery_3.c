#include "common.h"

#include <psyq/libgte.h>

#include "main/fs.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

extern u8           D_shelter_b6_nursery_80184CBC[];
extern u8           D_shelter_b6_nursery_80184CC4[];
extern u8           D_shelter_b6_nursery_80184CD0[];
extern u8           D_shelter_b6_nursery_80184CDC[];
extern UiObjectDesc D_800611E4;
extern u8           D_80071086;
extern UiObjectDesc D_shelter_b6_nursery_80184F70;
extern UiObjectDesc D_shelter_b6_nursery_80184F8C;

void func_shelter_b6_nursery_8017F170(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CBC,
                    prompt->field_1C, 1, 0);
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

void func_shelter_b6_nursery_8017F254(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_shelter_b6_nursery_80184CC4, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F70, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_3", func_shelter_b6_nursery_8017F31C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_3", func_shelter_b6_nursery_8017F3E4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_3", D_shelter_b6_nursery_8017D6A4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_3", jtbl_shelter_b6_nursery_8017D6B4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_3", jtbl_shelter_b6_nursery_8017D6CC);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_3", jtbl_shelter_b6_nursery_8017D70C);
