#include "common.h"

#include <psyq/libmcrd.h>

#include "main/unknown_syms.h"
#include "main/pad.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/mc.h"

static void McMenu_ConfirmDialog(UiList* arg0, UiObject* arg1);
static void McMenu_ConfirmNo(UiList* arg0, UiObject* arg1);
static void McMenu_ConfirmDialogAlt(UiList* arg0, UiObject* arg1);
static void McMenu_ConfirmYes(UiList* arg0, UiObject* arg1);
static void McMenu_InitByMode(Task* arg0);

static const char D_80013B64[] = "Select";
const char        D_80013B6C[] = "TIME";
const char        D_80013B74[] = "CLEAR";
const char        D_80013B7C[] = "Nightmare";
const char        D_80013B88[] = "Scavenger";
const char        D_80013B94[] = "Bounty";
const char        D_80013B9C[] = "Replay";
const char        D_80013BA4[] = " (";
const char        D_80013BA8[] = "EXP";
const char        D_80013BAC[] = "---";
const char        D_80013BB0[] = "BP";

static UiListItemFunc D_80061254[] = { McMenu_ConfirmDialog, McMenu_ConfirmNo };
static UiList         D_8006125C   = { D_80061254, 2, 2, 0, 0x0F };
static UiListItemFunc D_80061280[] = { McMenu_ConfirmDialogAlt };
static UiList         D_80061284   = { D_80061280, 1, 1, 0, 0x0F };
static UiListItemFunc D_800612A8[] = { McMenu_ConfirmYes };
static UiList         D_800612AC   = { D_800612A8, 1, 1, 0, 0x0F };
UiObjectDesc          D_800612D0[] = {
    { 0, 0, 0, 0x4B, 0x20, 0x10, 0, 0, 0xC0, McMenu_InitByMode, 0 },
};

void func_80036A1C(void)
{
    char pad[0x10];
}

static void McMenu_UpdateListCursor(void* arg0, UiPanel* arg1)
{
    Ui_UpdateListNoAnim(arg0, arg1);
    if (arg1->field_0.w == 1) {
        Ui_SmoothCursor(arg1, arg1->field_1C.s + 2, 0);
    }
}

void McMenu_SelectList(Task* arg0)
{
    UiPanel* obj;
    UiList*  menu;

    obj  = arg0->spawnArg2;
    menu = &D_8006116C;
    Ui_DrawText(obj, D_80013B64);
    if (arg0->state == 0) {
        Ui_InitList(menu, obj);
        menu->field_A   = 1;
        menu->field_10  = 0;
        menu->field_9.u = 0;
        Ui_SetListScrollFlag(menu, 1);
        arg0->state += 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->field_0.w == 1) {
            Ui_SmoothCursor(obj, obj->field_1C.s + 2, 0);
        }
    }
}

void McMenu_ConfirmWithRender(UiList* arg0, UiObject* arg1)
{
    s16 var_v0;
    s32 temp;
    s8  temp2;

    temp2 = arg0->field_8;
    temp  = arg1->owner->spawnArg1;
    func_800330D8(arg1, temp, temp2, 0, arg0->field_1A + 7);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            arg1->field_2E = 6;
            var_v0         = (s8)(u8)arg0->field_8;
            goto block_5;
        }
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            arg1->field_2E = 6;
            var_v0         = -1;
        block_5:
            arg1->field_2C = var_v0;
        }
    }
}

void McMenu_SelectListAlt(Task* arg0)
{
    UiPanel*          obj;
    UiList*           menu;
    WipSelectMenuExt* ctx;
    s32               temp;

    obj  = arg0->spawnArg2;
    ctx  = (WipSelectMenuExt*)arg0->spawnArg1;
    menu = &D_80061194;
    Ui_DrawText(obj, D_80013B64);
    if (arg0->state == 0) {
        Ui_InitList(menu, obj);
        menu->field_A   = 1;
        menu->field_10  = ctx->field_290;
        temp            = (u8)menu->field_10 - menu->field_5.u + 1;
        menu->field_9.u = temp;
        if ((s8)temp < 0) {
            menu->field_9.u = 0;
        }
        Ui_SetListScrollFlag(menu, 1);
        arg0->state += 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->field_0.w == 1) {
            Ui_SmoothCursor(obj, obj->field_1C.s + 2, 0);
        }
    }
}

void McMenu_FileInformation(Task* arg0)
{
    void*   obj;
    s32     data;
    UiList* menu;
    s32     val;

    obj = arg0->spawnArg2;
    if (arg0->state == 0) {
        arg0->killCountdown = (u16)arg0->spawnArg1;
        data                = arg0->parent->spawnArg1;
        arg0->state        += 1;
        arg0->spawnArg1     = data;
    }
    data = arg0->spawnArg1;
    Ui_DrawTitle(obj, "File Information");
    if (arg0->killCountdown == 1) {
        menu = &D_80061194;
    } else {
        menu = &D_8006116C;
    }
    val = menu->field_10;
    func_800330D8(obj, data, val, 0, 0);
}

static void McMenu_ConfirmDialog(UiList* arg0, UiObject* arg1)
{
    s32 temp;

    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_80060A54, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(0x15, 0, 0);
            arg0->field_B  = temp;
            arg0->field_22 = 0x41;
        }
    }
}

static void McMenu_ConfirmDialogAlt(UiList* arg0, UiObject* arg1)
{
    s32 temp;

    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_80060A64, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        }
    }
}

static void McMenu_ConfirmYes(UiList* arg0, UiObject* arg1)
{
    s32 temp;

    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_80060A5C, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        }
    }
}

static void McMenu_ConfirmNo(UiList* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_80060A58, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = -1;
        }
    }
}

static void McMenu_InitByMode(Task* arg0)
{
    UiPanel* obj;
    UiList*  menu;
    s32      mode;

    mode = arg0->spawnArg1;
    obj  = arg0->spawnArg2;
    if (mode == 2) {
        goto block_2;
    }
    if (mode >= 3) {
        goto block_default;
    }
    if (mode != 1) {
        goto block_default;
    }
    menu = &D_80061284;
    goto block_done;
block_2:
    menu = &D_800612AC;
    goto block_done;
block_default:
    menu = &D_8006125C;
block_done:
    if (arg0->state == 0) {
        Ui_LayoutListPanel(menu, obj);
        obj->bounds.rect.y -= obj->bounds.rect.h / 2;
        if (arg0->spawnArg1 != 3) {
            menu->field_10 = 0;
        } else {
            menu->field_10 = 1;
        }
        menu->field_9.u = 0;
        Ui_SetListScrollFlag(menu, 1);
        arg0->state += 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
    }
}
