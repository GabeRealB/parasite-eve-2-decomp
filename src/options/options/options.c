#include "common.h"

#include <psyq/libgte.h>

#include "main/mc.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"

extern u8           D_options_801D5B2C[];
extern u8           D_options_801D5BAC[];
extern u8           D_options_801D5DA4[];
extern u8           D_options_801D5DDC[];
extern UiObjectDesc D_options_801D5EFC;

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D404C);

INCLUDE_RODATA("options/nonmatchings/options/options", D_options_801D4000);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D42A8);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D4504);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D4724);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D4944);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D4B64);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D4D0C);

void func_options_801D5954(DialogPrompt* arg0, UiObject* arg1)
{
    s32 status;

    Text_DrawPrompt(arg1, arg1->field_1C + 6, arg0->field_1A, D_options_801D5BAC, arg0->field_1C, 1, 0);
    status = arg1->status;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam((s32)D_options_801D5DA4, 0, 0);
    }
    if ((arg0->field_C == 1) && (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0)) {
        SndEvt_EnqueueType6(3, 0, 0);
        Ui_SpawnFromDesc(&D_options_801D5EFC, 0, 1, 1, arg1);
        arg1->status = 0;
    }
}

void func_options_801D5A4C(DialogPrompt* arg0, UiObject* arg1)
{
    s32 status;

    Text_DrawPrompt(arg1, arg1->field_1C + 6, arg0->field_1A, D_options_801D5B2C, arg0->field_1C, 1, 0);
    status = arg1->status;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam((s32)D_options_801D5DDC, 0, 0);
    }
    if ((arg0->field_C == 1) && (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0)) {
        SndEvt_EnqueueType6(3, 0, 0);
        Mc_ResetSaveFlags();
    }
}
