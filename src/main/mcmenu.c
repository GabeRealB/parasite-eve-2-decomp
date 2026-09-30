#include "ui.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "mc.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

static const char McText_Select[];

static UiListItemFunc Mc_YesNoCallbacks[];

static UiList Mc_YesNoList;

static UiListItemFunc Mc_OkCallbacks[];

static UiList Mc_OkList;

static UiListItemFunc Mc_YesCallbacks[];

static UiList Mc_YesList;

static void McMenu_UpdateListCursor(void* arg0, UiPanel* panel);

static void McMenu_ConfirmDialog(UiList* list, UiObject* object);

static void McMenu_ConfirmDialogAlt(UiList* list, UiObject* object);

static void McMenu_ConfirmYes(UiList* list, UiObject* object);

static void McMenu_ConfirmNo(UiList* list, UiObject* object);

static void McMenu_InitByMode(Task* task);

static const char McText_Select[]      = "Select";
const char        McText_Time[]        = "TIME";
const char        McText_Clear[]       = "CLEAR";
const char        McText_Nightmare[]   = "Nightmare";
const char        McText_Scavenger[]   = "Scavenger";
const char        McText_Bounty[]      = "Bounty";
const char        McText_Replay[]      = "Replay";
const char        McText_OpenParen[]   = " (";
const char        McText_Exp[]         = "EXP";
const char        McText_Unavailable[] = "---";
const char        McText_Bp[]          = "BP";

static UiListItemFunc Mc_YesNoCallbacks[] = { McMenu_ConfirmDialog, McMenu_ConfirmNo };
static UiList         Mc_YesNoList        = { Mc_YesNoCallbacks, 2, 2, 0, 0x0F };
static UiListItemFunc Mc_OkCallbacks[]    = { McMenu_ConfirmDialogAlt };
static UiList         Mc_OkList           = { Mc_OkCallbacks, 1, 1, 0, 0x0F };
static UiListItemFunc Mc_YesCallbacks[]   = { McMenu_ConfirmYes };
static UiList         Mc_YesList          = { Mc_YesCallbacks, 1, 1, 0, 0x0F };
UiObjectDesc          Mc_PromptDesc[]     = {
    { 0, 0, 0, 0x4B, 0x20, 0x10, 0, 0, 0xC0, McMenu_InitByMode, 0 },
};

void McMenu_NoOpTask(Task* unused)
{
    char pad[0x10];
}

static void McMenu_UpdateListCursor(void* arg0, UiPanel* panel)
{
    Ui_UpdateListNoAnim(arg0, panel);
    if (panel->field_0.w == 1) {
        Ui_SmoothCursor(panel, panel->field_1C.signedValue + 2, 0);
    }
}

void McMenu_SelectList(Task* task)
{
    UiPanel* obj;
    UiList*  menu;

    obj  = task->spawnArg2.pointer;
    menu = &Mc_SaveSlotList;
    Ui_DrawText(obj, McText_Select);
    if (task->state == 0) {
        Ui_InitList(menu, obj);
        menu->field_A   = 1;
        menu->field_10  = 0;
        menu->field_9.u = 0;
        Ui_SetListScrollFlag(menu, 1);
        task->state += 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->field_0.w == 1) {
            Ui_SmoothCursor(obj, obj->field_1C.signedValue + 2, 0);
        }
    }
}

void McMenu_ConfirmWithRender(UiList* list, UiObject* object)
{
    s16     var_v0;
    McWork* temp;
    s8      temp2;

    temp2 = list->field_8;
    temp  = object->owner->spawnArg1.pointer;
    Mc_DrawSlotDetails(object, temp, temp2, 0, list->field_1A + 7);
    if (list->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            object->field_2E = 6;
            var_v0           = (s8)(u8)list->field_8;
            goto block_5;
        }
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            object->field_2E = 6;
            var_v0           = -1;
        block_5:
            object->field_2C = var_v0;
        }
    }
}

void McMenu_SelectListAlt(Task* task)
{
    UiPanel* obj;
    UiList*  menu;
    McWork*  ctx;
    s32      temp;

    obj  = task->spawnArg2.pointer;
    ctx  = task->spawnArg1.pointer;
    menu = &Mc_LoadSlotList;
    Ui_DrawText(obj, McText_Select);
    if (task->state == 0) {
        Ui_InitList(menu, obj);
        menu->field_A   = 1;
        menu->field_10  = ctx->selectedSlot;
        temp            = (u8)menu->field_10 - menu->field_5.u + 1;
        menu->field_9.u = temp;
        if ((s8)temp < 0) {
            menu->field_9.u = 0;
        }
        Ui_SetListScrollFlag(menu, 1);
        task->state += 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->field_0.w == 1) {
            Ui_SmoothCursor(obj, obj->field_1C.signedValue + 2, 0);
        }
    }
}

void McMenu_FileInformation(Task* task)
{
    void*   obj;
    McWork* data;
    UiList* menu;
    s32     val;

    obj = task->spawnArg2.pointer;
    if (task->state == 0) {
        task->killCountdown     = (u16)task->spawnArg1.value;
        data                    = task->parent->spawnArg1.pointer;
        task->state            += 1;
        task->spawnArg1.pointer = data;
    }
    data = task->spawnArg1.pointer;
    Ui_DrawTitle(obj, "File Information");
    if (task->killCountdown == 1) {
        menu = &Mc_LoadSlotList;
    } else {
        menu = &Mc_SaveSlotList;
    }
    val = menu->field_10;
    Mc_DrawSlotDetails(obj, data, val, 0, 0);
}

static void McMenu_ConfirmDialog(UiList* list, UiObject* object)
{
    s32 temp;

    Text_DrawPrompt(object, list->field_18, list->field_1A, McText_Yes, list->field_1C, 1, 0);
    temp = list->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            object->field_2E = 6;
            object->field_2C = temp;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(0x15, 0, 0);
            list->field_B  = temp;
            list->field_22 = 0x41;
        }
    }
}

static void McMenu_ConfirmDialogAlt(UiList* list, UiObject* object)
{
    s32 temp;

    Text_DrawPrompt(object, list->field_18, list->field_1A, McText_Ok, list->field_1C, 1, 0);
    temp = list->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            object->field_2E = 6;
            object->field_2C = temp;
        }
    }
}

static void McMenu_ConfirmYes(UiList* list, UiObject* object)
{
    s32 temp;

    Text_DrawPrompt(object, list->field_18, list->field_1A, McText_Cancel, list->field_1C, 1, 0);
    temp = list->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            object->field_2E = 6;
            object->field_2C = temp;
        }
    }
}

static void McMenu_ConfirmNo(UiList* list, UiObject* object)
{
    Text_DrawPrompt(object, list->field_18, list->field_1A, McText_No, list->field_1C, 1, 0);
    if (list->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            object->field_2E = 6;
            object->field_2C = -1;
        }
    }
}

static void McMenu_InitByMode(Task* task)
{
    UiPanel* obj;
    UiList*  menu;
    s32      mode;

    mode = task->spawnArg1.value;
    obj  = task->spawnArg2.pointer;
    if (mode == 2) {
        goto block_2;
    }
    if (mode >= 3) {
        goto block_default;
    }
    if (mode != 1) {
        goto block_default;
    }
    menu = &Mc_OkList;
    goto block_done;
block_2:
    menu = &Mc_YesList;
    goto block_done;
block_default:
    menu = &Mc_YesNoList;
block_done:
    if (task->state == 0) {
        Ui_LayoutListPanel(menu, obj);
        obj->bounds.rect.y -= obj->bounds.rect.h / 2;
        if (task->spawnArg1.value != 3) {
            menu->field_10 = 0;
        } else {
            menu->field_10 = 1;
        }
        menu->field_9.u = 0;
        Ui_SetListScrollFlag(menu, 1);
        task->state += 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
    }
}
