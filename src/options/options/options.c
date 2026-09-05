#include "common.h"

#include <psyq/libgte.h>

#include "main/fs.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"

extern s8           D_80072189;
extern u8           D_8007218D;
extern s8           D_80072311;
extern s8           D_80072313;
extern u8           D_options_801D5B2C[];
extern u8           D_options_801D5B4C[];
extern u8           D_options_801D5B58[];
extern u8           D_options_801D5B5C[];
extern u8           D_options_801D5B60[];
extern u8           D_options_801D5B68[];
extern u8           D_options_801D5B70[];
extern u8           D_options_801D5B90[];
extern u8           D_options_801D5B98[];
extern u8           D_options_801D5BA4[];
extern u8           D_options_801D5BAC[];
extern u8           D_options_801D5BB8[];
extern u8           D_options_801D5BC4[];
extern u8           D_options_801D5BCC[];
extern u8           D_options_801D5C7C[];
extern u8           D_options_801D5CE4[];
extern u8           D_options_801D5D28[];
extern u8           D_options_801D5D68[];
extern u8           D_options_801D5DA4[];
extern u8           D_options_801D5DDC[];
extern UiList       D_options_801D5EB0;
extern UiList       D_options_801D5ED8;
extern UiObjectDesc D_options_801D5EFC;

void func_options_801D404C(DialogPrompt* arg0, UiObject* arg1)
{
    u8*          labels[2];
    u8**         p;
    u8*          title;
    s32          a0tmp;
    s32          i;
    s32          y;
    s32          x;
    s32          span;
    s32          selected;
    s32          one;
    s32          look;
    s32          status;
    s32          saved;
    s32          cur;
    s32          tmp;
    register s32 two asm("t0");

    SCHED_BARRIER();
    a0tmp = (s32)arg1;
    TOUCH_REG(a0tmp);
    title = D_options_801D5B60;
    TOUCH_REG(title);
    i         = 0;
    p         = labels;
    labels[0] = D_options_801D5B68;
    labels[1] = D_options_801D5B70;
    Text_DrawPrompt((UiObject*)a0tmp, arg1->field_1C + 6, arg0->field_1A, title, arg0->field_1C, 1, 0);
    SCHED_BARRIER();
    saved    = D_80072311;
    y        = i;
    selected = saved;
    x        = arg1->field_1C + 0x78;
    span     = (s16)arg1->field_1E - x;
    do {
        if (i != selected) {
            look = Ui_LookupTable(arg1, 2);
        } else {
            look = Ui_LookupTable(arg1, 1);
        }
        one = 1;
        two = 2;
        Text_DrawPrompt(arg1, x + y / two, arg0->field_1A, *p, look, one, 0);
        p++;
        y += span;
        i += one;
    } while (i < 2);
    if (arg0->field_C == one) {
        if (Pad_CheckButtons(0, one, 0x2000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected += one;
            if (selected >= 2) {
                selected = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected -= 1;
            if (selected < 0) {
                selected += 2;
            }
        }
    }
    D_80072311 = selected;
    SOFT_BARRIER();
    tmp = selected << 24;
    SOFT_TOUCH_REG(tmp);
    two = saved;
    SOFT_TOUCH_REG(two);
    cur = tmp >> 24;
    if (two != cur) {
        if (cur != 0) {
            if (cur != 1) {
                CdVol_SetMixMode(1);
            } else {
                CdVol_SetMixMode(0);
            }
        } else {
            CdVol_SetMixMode(1);
        }
    }
    SOFT_BARRIER();
    SOFT_BARRIER();
    SOFT_BARRIER();
    SOFT_BARRIER();
    status = arg1->status;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam((s32)D_options_801D5C7C, 0, 0);
    }
}

INCLUDE_RODATA("options/nonmatchings/options/options", D_options_801D4000);

INCLUDE_ASM("options/nonmatchings/options/options", func_options_801D42A8);

void func_options_801D4504(DialogPrompt* arg0, UiObject* arg1)
{
    u8*  labels[2];
    u8** p;
    u8*  title;
    s32  a0tmp;
    s32  i;
    s32  y;
    s32  x;
    s32  span;
    s32  selected;
    s32  one;
    s32  two;
    s32  look;
    s32  n2;
    s32  status;

    SCHED_BARRIER();
    a0tmp = (s32)arg1;
    TOUCH_REG(a0tmp);
    title = D_options_801D5B90;
    TOUCH_REG(title);
    i         = 0;
    p         = labels;
    y         = i;
    labels[0] = D_options_801D5BA4;
    labels[1] = D_options_801D5B98;
    Text_DrawPrompt((UiObject*)a0tmp, arg1->field_1C + 6, arg0->field_1A, title, arg0->field_1C, 1, 0);
    selected = D_80072313;
    x        = arg1->field_1C + 0x78;
    span     = (s16)arg1->field_1E - x;
    n2       = 2;
    do {
        if (i != selected) {
            look = Ui_LookupTable(arg1, 2);
        } else {
            look = Ui_LookupTable(arg1, 1);
        }
        one = 1;
        two = n2;
        Text_DrawPrompt(arg1, x + y / two, arg0->field_1A, *p, look, one, 0);
        p++;
        y += span;
        i += one;
    } while (i < 2);
    if (arg0->field_C == one) {
        if (Pad_CheckButtons(0, one, 0x2000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected += one;
            if (selected >= n2) {
                selected = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected -= 1;
            if (selected < 0) {
                selected += n2;
            }
        }
    }
    D_80072313 = selected;
    SOFT_BARRIER();
    status = arg1->status;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam((s32)D_options_801D5CE4, 0, 0);
    }
}

void func_options_801D4724(DialogPrompt* arg0, UiObject* arg1)
{
    u8*  labels[2];
    u8** p;
    u8*  title;
    s32  a0tmp;
    s32  i;
    s32  y;
    s32  x;
    s32  span;
    s32  selected;
    s32  one;
    s32  two;
    s32  look;
    s32  n2;
    s32  status;

    SCHED_BARRIER();
    a0tmp = (s32)arg1;
    TOUCH_REG(a0tmp);
    title = D_options_801D5B4C;
    TOUCH_REG(title);
    i         = 0;
    p         = labels;
    y         = i;
    labels[0] = D_options_801D5B58;
    labels[1] = D_options_801D5B5C;
    Text_DrawPrompt((UiObject*)a0tmp, arg1->field_1C + 6, arg0->field_1A, title, arg0->field_1C, 1, 0);
    selected = D_80072189;
    x        = arg1->field_1C + 0x78;
    span     = (s16)arg1->field_1E - x;
    n2       = 2;
    do {
        if (i != selected) {
            look = Ui_LookupTable(arg1, 2);
        } else {
            look = Ui_LookupTable(arg1, 1);
        }
        one = 1;
        two = n2;
        Text_DrawPrompt(arg1, x + y / two, arg0->field_1A, *p, look, one, 0);
        p++;
        y += span;
        i += one;
    } while (i < 2);
    if (arg0->field_C == one) {
        if (Pad_CheckButtons(0, one, 0x2000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected += one;
            if (selected >= n2) {
                selected = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected -= 1;
            if (selected < 0) {
                selected += n2;
            }
        }
    }
    D_80072189 = selected;
    SOFT_BARRIER();
    status = arg1->status;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam((s32)D_options_801D5D28, 0, 0);
    }
}

void func_options_801D4944(DialogPrompt* arg0, UiObject* arg1)
{
    u8*  labels[2];
    u8** p;
    u8*  title;
    s32  a0tmp;
    s32  i;
    s32  y;
    s32  x;
    s32  span;
    s32  selected;
    s32  one;
    s32  two;
    s32  look;
    s32  n2;
    s32  status;

    SCHED_BARRIER();
    a0tmp = (s32)arg1;
    TOUCH_REG(a0tmp);
    title = D_options_801D5BB8;
    TOUCH_REG(title);
    i         = 0;
    p         = labels;
    y         = i;
    labels[0] = D_options_801D5BC4;
    labels[1] = D_options_801D5BCC;
    Text_DrawPrompt((UiObject*)a0tmp, arg1->field_1C + 6, arg0->field_1A, title, arg0->field_1C, 1, 0);
    selected = D_8007218D;
    x        = arg1->field_1C + 0x78;
    span     = (s16)arg1->field_1E - x;
    n2       = 2;
    do {
        if (i != selected) {
            look = Ui_LookupTable(arg1, 2);
        } else {
            look = Ui_LookupTable(arg1, 1);
        }
        one = 1;
        two = n2;
        Text_DrawPrompt(arg1, x + y / two, arg0->field_1A, *p, look, one, 0);
        p++;
        y += span;
        i += one;
    } while (i < 2);
    if (arg0->field_C == one) {
        if (Pad_CheckButtons(0, one, 0x2000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected += one;
            if (selected >= n2) {
                selected = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected -= 1;
            if (selected < 0) {
                selected += n2;
            }
        }
    }
    D_8007218D = selected;
    SOFT_BARRIER();
    status = arg1->status;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam((s32)D_options_801D5D68, 0, 0);
    }
}

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
