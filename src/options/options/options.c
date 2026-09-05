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
extern UiList       D_options_801D5EB0;
extern UiList       D_options_801D5ED8;
extern UiObjectDesc D_options_801D5EFC;

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D404C);

INCLUDE_RODATA("options/nonmatchings/options/options", D_options_801D4000);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D42A8);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D4504);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D4724);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D4944);

void func_options_801D4B64(Task* task)
{
    UiList*   list;
    UiObject* obj;
    UiObject* child;
    s32       status;
    s32       result;

    list = &D_options_801D5EB0;
    obj  = task->spawnArg2;
    if (task->spawnArg1 == 1) {
        list = &D_options_801D5ED8;
    }
    if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        task->state += 1;
        if (task->spawnArg1 == 1) {
            Ui_UpdateLayoutSize((UiPanel*)obj, 0xC0, 0);
            obj->field_E = -((s16)obj->field_12 / 2);
            obj->field_C = -((s16)obj->field_10 / 2);
        }
    }
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, "Option");
    Ui_UpdateListNoAnim(list, obj);
    status = obj->status;
    if (status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2C = status;
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }
    if (task->firstChild != NULL) {
        child  = task->firstChild->spawnArg2;
        result = child->field_2E;
        switch (result) {
            case 6:
                Ui_TeardownTree(child, child->owner);
                obj->status = 1;
                break;
            case -1:
                obj->field_2E = result;
                break;
        }
    }
}

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
