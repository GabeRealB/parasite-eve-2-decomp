#include "common.h"
#include "main/mc.h"

#include <psyq/libmcrd.h>

#include "main/game.h"
#include "main/mem.h"
#include "main/unknown_syms.h"

void func_80036A1C(void)
{
    char pad[0x10];
}

void McMenu_UpdateListCursor(void* arg0, UiMiniObj* arg1)
{
    Ui_UpdateListNoAnim(arg0, arg1);
    if (arg1->field_0 == 1) {
        Ui_SmoothCursor(arg1, arg1->field_1c + 2, 0);
    }
}

void McMenu_SelectList(Task* arg0)
{
    UiMiniObj* obj;
    UiList*    menu;

    obj  = arg0->spawnArg2;
    menu = &D_8006116C;
    Ui_DrawText((UiPanel*)obj, D_80013B64);
    if (arg0->state == 0) {
        Ui_InitList(menu, obj);
        menu->field_A  = 1;
        menu->field_10 = 0;
        menu->field_9  = 0;
        Ui_SetListScrollFlag(menu, 1);
        arg0->state += 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->field_0 == 1) {
            Ui_SmoothCursor(obj, obj->field_1c + 2, 0);
        }
    }
}

void McMenu_ConfirmWithRender(DialogPrompt* arg0, UiObject* arg1)
{
    s16 var_v0;
    s32 temp;
    s8  temp2;

    temp2 = arg0->field_8;
    temp  = arg1->owner->spawnArg1;
    func_800330D8(arg1, temp, temp2, 0, arg0->field_1A + 7);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            arg1->field_2E = 6;
            var_v0         = (s8)(u8)arg0->field_8;
            goto block_5;
        }
        if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
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
    UiMiniObj*        obj;
    UiList*           menu;
    WipSelectMenuExt* ctx;
    s32               temp;

    obj  = arg0->spawnArg2;
    ctx  = (WipSelectMenuExt*)arg0->spawnArg1;
    menu = &D_80061194;
    Ui_DrawText((UiPanel*)obj, D_80013B64);
    if (arg0->state == 0) {
        Ui_InitList(menu, obj);
        menu->field_A  = 1;
        menu->field_10 = ctx->field_290;
        temp           = (u8)menu->field_10 - menu->field_5 + 1;
        menu->field_9  = temp;
        if ((s8)temp < 0) {
            menu->field_9 = 0;
        }
        Ui_SetListScrollFlag(menu, 1);
        arg0->state += 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
        if (obj->field_0 == 1) {
            Ui_SmoothCursor(obj, obj->field_1c + 2, 0);
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
    Ui_DrawTitle(obj, D_80013BB4);
    if (arg0->killCountdown == 1) {
        menu = &D_80061194;
    } else {
        menu = &D_8006116C;
    }
    val = menu->field_10;
    func_800330D8(obj, data, val, 0, 0);
}

void McMenu_ConfirmDialog(DialogPrompt* arg0, UiObject* arg1)
{
    s32 temp;

    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_80060A54, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            SndEvt_EnqueueType6(0x15, 0, 0);
            arg0->field_B  = temp;
            arg0->field_22 = 0x41;
        }
    }
}

void McMenu_ConfirmDialogAlt(DialogPrompt* arg0, UiObject* arg1)
{
    s32 temp;

    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_80060A64, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        }
    }
}

void McMenu_ConfirmYes(DialogPrompt* arg0, UiObject* arg1)
{
    s32 temp;

    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_80060A5C, arg0->field_1C, 1, 0);
    temp = arg0->field_C;
    if (temp == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = temp;
        }
    }
}

void McMenu_ConfirmNo(DialogPrompt* arg0, UiObject* arg1)
{
    Text_DrawPrompt(arg1, arg0->field_18, arg0->field_1A, D_80060A58, arg0->field_1C, 1, 0);
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED70 | D_8005ED74) != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = -1;
        }
    }
}

void McMenu_InitByMode(Task* arg0)
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
        obj->field_C.y -= obj->field_C.h / 2;
        if (arg0->spawnArg1 != 3) {
            menu->field_10 = 0;
        } else {
            menu->field_10 = 1;
        }
        menu->field_9 = 0;
        Ui_SetListScrollFlag(menu, 1);
        arg0->state += 1;
    } else {
        Ui_UpdateListNoAnim(menu, obj);
    }
}
