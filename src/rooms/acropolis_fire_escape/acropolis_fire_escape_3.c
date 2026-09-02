#include "common.h"

#include <psyq/libgte.h>

#include "main/fs.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

extern u8           D_acropolis_fire_escape_801819F8[];
extern u8           D_acropolis_fire_escape_80181A00[];
extern u8           D_acropolis_fire_escape_80181A0C[];
extern u8           D_acropolis_fire_escape_80181A18[];
extern u8           D_80071086;
extern UiObjectDesc D_800611E4;
extern UiObjectDesc D_acropolis_fire_escape_80181CAC;
extern UiObjectDesc D_acropolis_fire_escape_80181CC8;

void func_acropolis_fire_escape_8017F114(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_fire_escape_801819F8,
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
INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_3", func_acropolis_fire_escape_8017F1F8);
INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_3", func_acropolis_fire_escape_8017F2C0);
INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape_3", func_acropolis_fire_escape_8017F388);