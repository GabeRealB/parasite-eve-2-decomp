#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

/// Menu labels and help text for the options screen.
static u8 D_options_801D5B2C[32] = "Restore default configuration";
static u8 D_options_801D5B4C[12] = "Vibration";
static u8 D_options_801D5B58[4]  = "On";
static u8 D_options_801D5B5C[4]  = "Off";
static u8 D_options_801D5B60[8]  = "Sound";
static u8 D_options_801D5B68[8]  = "Stereo";
static u8 D_options_801D5B70[8]  = "Mono";
static u8 D_options_801D5B78[8]  = "Music";
static u8 D_options_801D5B80[4]  = "3";
static u8 D_options_801D5B84[4]  = "2";
static u8 D_options_801D5B88[4]  = "1";
static u8 D_options_801D5B8C[4]  = "Off";
static u8 D_options_801D5B90[8]  = "Cursor";
static u8 D_options_801D5B98[12] = "Standard";
static u8 D_options_801D5BA4[8]  = "Memory";
static u8 D_options_801D5BAC[12] = "Key Config";
static u8 D_options_801D5BB8[12] = "Movement";
static u8 D_options_801D5BC4[8]  = "Walk";
static u8 D_options_801D5BCC[4]  = "Run";
static u8 D_options_801D5BD0[4]  = "OK";
static u8 D_options_801D5BD4[8]  = "Cancel";
static u8 D_options_801D5BDC[8]  = "Help";
static u8 D_options_801D5BE4[12] = "Scroll Down";
static u8 D_options_801D5BF0[12] = "Scroll Up";
static u8 D_options_801D5BFC[12] = "Draw Weapon";
static u8 D_options_801D5C08[8]  = "Examine";
static u8 D_options_801D5C10[4]  = "Run";
static u8 D_options_801D5C14[8]  = "Walk";
static u8 D_options_801D5C1C[16] = "Switch Targets";
static u8 D_options_801D5C2C[8]  = "PE Menu";
static u8 D_options_801D5C34[12] = "Main Weapon";
static u8 D_options_801D5C40[12] = "Sub Weapon";
static u8 D_options_801D5C4C[8]  = "MENU";
static u8 D_options_801D5C54[8]  = "NORMAL";
static u8 D_options_801D5C5C[8]  = "BATTLE";
static u8 D_options_801D5C64[8]  = "Type A";
static u8 D_options_801D5C6C[8]  = "Type B";
static u8 D_options_801D5C74[8]  = "Type C";
static u8 D_options_801D5C7C[44] = "Set sound output to match\nyour television.";
static u8 D_options_801D5CA8[60] = "Set music volume level.\nSet low for clearer sound effects.";
static u8 D_options_801D5CE4[68] = "Set menu cursor. You can have it\nremember the last position used.";
static u8 D_options_801D5D28[64] = "Set vibration mode ON/OFF.\nSelect ON for more realistic play.";
static u8 D_options_801D5D68[60] = "Set Aya's default movement.\nBeginners should use \"Walk.\"";
static u8 D_options_801D5DA4[56] = "Open key settings menu.\nPress the } button to proceed.";
static u8 D_options_801D5DDC[48] = "Restore default settings.\nPress the } button.";
static u8 D_options_801D5E0C[20] = "Default setting.";
static u8 D_options_801D5E20[56] = "The function of the { button\nand ~ button is switched.";
static u8 D_options_801D5E58[56] = "This mode uses the ~ button and\n | button for combat.";

/// Forward declarations for the list-item tables below; these are defined
/// later in this unit.
static void func_options_801D404C(UiList* arg0, UiObject* arg1);
static void func_options_801D42A8(UiList* arg0, UiObject* arg1);
static void func_options_801D4504(UiList* arg0, UiObject* arg1);
static void func_options_801D4724(UiList* arg0, UiObject* arg1);
static void func_options_801D4944(UiList* arg0, UiObject* arg1);
static void func_options_801D5954(UiList* arg0, UiObject* arg1);
static void func_options_801D5A4C(UiList* arg0, UiObject* arg1);
static void func_options_801D4D0C(Task* task);

/// Sits immediately before the list tables; zero on disc.
static u8 D_options_801D5E90[4] = { 0, 0, 0, 0 };

/// The seven list-item renderers the main options list dispatches through.
static UiListItemFunc D_options_801D5E94[7] = {
    func_options_801D404C,
    func_options_801D42A8,
    func_options_801D4504,
    func_options_801D4724,
    func_options_801D4944,
    func_options_801D5954,
    func_options_801D5A4C,
};

/// The main options list: seven rows of 0x12 pixels.
static UiList D_options_801D5EB0 = { D_options_801D5E94, 0x07, 0x07, 0x00, 0x12 };

/// The key-config sub-list renders every row with the same function.
static UiListItemFunc D_options_801D5ED4[1] = { func_options_801D4724 };

/// That sub-list: one row of 0x0F pixels.
static UiList D_options_801D5ED8 = { D_options_801D5ED4, 0x01, 0x01, 0x00, 0x0F };

/// The options screen's own UI object, spawned by `Ui_SpawnFromDesc`.
static UiObjectDesc D_options_801D5EFC = {
    2,
    0xFF74,
    0xFFA0,
    0x0118,
    0x0090,
    0x0034,
    0x0000,
    0x0000,
    0x00C0,
    func_options_801D4D0C,
    0,
};

/// Key-icon UV pairs for the key configuration screen, one (u, v) per row.
typedef struct {
    u8 pairs[8][2];
} KeyIconUvs;

/// The last text request's storage is reused as the per-row UV scratch buffer
/// (both 16 bytes), which is what keeps the frame at 0x120.
typedef union {
    TextDrawReq req;
    KeyIconUvs  uvs;
} KeyIconReq;

static const KeyIconUvs Options_KeyIconUvs;

static void func_options_801D4B64(Task* task);

static void func_options_801D404C(UiList* arg0, UiObject* arg1)
{
    u8*  labels[2] = { D_options_801D5B68, D_options_801D5B70 };
    u8** p;
    u8*  title;
    s32  i;
    s32  y;
    s32  x;
    s32  span;
    s32  selected;
    s32  one;
    s32  look;
    s32  status;
    s32  saved;
    s32  cur;
    s32  columnCount;

    columnCount = 2;
    title       = D_options_801D5B60;
    Text_DrawPrompt(arg1, arg1->panel.contentLeft.signedValue + 6, arg0->field_1A, title, arg0->field_1C, 1, 0);
    i        = 0;
    p        = labels;
    saved    = Mc_SaveData[0].state.soundMode;
    y        = i;
    selected = saved;
    x        = arg1->panel.contentLeft.signedValue + 0x78;
    span     = arg1->panel.contentRight.signedValue - x;
    do {
        if (i != selected) {
            look = Ui_LookupTable(arg1, 2);
        } else {
            look = Ui_LookupTable(arg1, 1);
        }
        one = 1;
        Text_DrawPrompt(arg1, x + y / columnCount, arg0->field_1A, *p, look, one, 0);
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
    Mc_SaveData[0].state.soundMode = selected;
    cur                            = Mc_SaveData[0].state.soundMode;
    if (saved != cur) {
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
    status = arg1->panel.control.word;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam(D_options_801D5C7C, 0, 0);
    }
}

static void func_options_801D42A8(UiList* arg0, UiObject* arg1)
{
    u8* labels[4] = {
        D_options_801D5B80,
        D_options_801D5B84,
        D_options_801D5B88,
        D_options_801D5B8C,
    };
    u8**      p;
    u8*       title;
    UiObject* a0tmp;
    s32       i;
    s32       y;
    s32       x;
    s32       span;
    s32       selected;
    s32       saved;
    s32       one;
    s32       look;
    s32       count;
    s32       status;

    count = 4;
    a0tmp = arg1;
    title = D_options_801D5B78;
    Text_DrawPrompt(a0tmp, arg1->panel.contentLeft.signedValue + 6, arg0->field_1A, title, arg0->field_1C, 1, 0);
    i        = 0;
    p        = labels;
    saved    = Mc_SaveData[0].state.musicVolume;
    y        = i;
    selected = saved;
    x        = arg1->panel.contentLeft.signedValue + 0x78;
    span     = arg1->panel.contentRight.signedValue - x;
    do {
        if (i != selected) {
            look = Ui_LookupTable(arg1, 2);
        } else {
            look = Ui_LookupTable(arg1, 1);
        }
        one = 1;
        Text_DrawPrompt(arg1, x + y / count, arg0->field_1A, *p, look, one, 0);
        p++;
        y += span;
        i += one;
    } while (i < 4);
    if (arg0->field_C == one) {
        if (Pad_CheckButtons(0, one, 0x2000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected += one;
            if (selected >= count) {
                selected = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected -= 1;
            if (selected < 0) {
                selected += count;
            }
        }
    }
    Mc_SaveData[0].state.musicVolume = selected;
    if (saved != (s8)selected) {
        Snd_ApplyVolumeTable(0);
    }
    status = arg1->panel.control.word;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam(D_options_801D5CA8, 0, 0);
    }
}

static void func_options_801D4504(UiList* arg0, UiObject* arg1)
{
    u8*  labels[2] = { D_options_801D5BA4, D_options_801D5B98 };
    u8** p;
    s32  i;
    s32  y;
    s32  x;
    s32  span;
    s32  selected;
    s32  one;
    s32  look;
    s32  n2;
    s32  status;

    Text_DrawPrompt(arg1, arg1->panel.contentLeft.signedValue + 6, arg0->field_1A, D_options_801D5B90, arg0->field_1C, 1, 0);
    i        = 0;
    p        = labels;
    y        = i;
    selected = Mc_SaveData[0].state.cursorMode;
    x        = arg1->panel.contentLeft.signedValue + 0x78;
    span     = arg1->panel.contentRight.signedValue - x;
    n2       = 2;
    do {
        if (i != selected) {
            look = Ui_LookupTable(arg1, 2);
        } else {
            look = Ui_LookupTable(arg1, 1);
        }
        one = 1;
        Text_DrawPrompt(arg1, x + y / n2, arg0->field_1A, *p, look, one, 0);
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
    Mc_SaveData[0].state.cursorMode = selected;
    status                          = arg1->panel.control.word;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam(D_options_801D5CE4, 0, 0);
    }
}

static void func_options_801D4724(UiList* arg0, UiObject* arg1)
{
    u8*  labels[2] = { D_options_801D5B58, D_options_801D5B5C };
    u8** p;
    u8*  title;
    s32  i;
    s32  y;
    s32  x;
    s32  span;
    s32  selected;
    s32  one;
    s32  look;
    s32  n2;
    s32  status;

    title = D_options_801D5B4C;
    Text_DrawPrompt(arg1, arg1->panel.contentLeft.signedValue + 6, arg0->field_1A, title, arg0->field_1C, 1, 0);
    i        = 0;
    p        = labels;
    y        = i;
    selected = Mc_SaveData[0].state.vibration;
    x        = arg1->panel.contentLeft.signedValue + 0x78;
    span     = arg1->panel.contentRight.signedValue - x;
    n2       = 2;
    do {
        if (i != selected) {
            look = Ui_LookupTable(arg1, 2);
        } else {
            look = Ui_LookupTable(arg1, 1);
        }
        one = 1;
        Text_DrawPrompt(arg1, x + y / n2, arg0->field_1A, *p, look, one, 0);
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
    Mc_SaveData[0].state.vibration = selected;
    status                         = arg1->panel.control.word;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam(D_options_801D5D28, 0, 0);
    }
}

static void func_options_801D4944(UiList* arg0, UiObject* arg1)
{
    u8*  labels[2] = { D_options_801D5BC4, D_options_801D5BCC };
    u8** p;
    u8*  title;
    s32  i;
    s32  y;
    s32  x;
    s32  span;
    s32  selected;
    s32  one;
    s32  look;
    s32  columnCount;
    s32  status;

    title = D_options_801D5BB8;
    Text_DrawPrompt(arg1, arg1->panel.contentLeft.signedValue + 6, arg0->field_1A, title, arg0->field_1C, 1, 0);
    i           = 0;
    p           = labels;
    y           = i;
    selected    = Mc_SaveData[0].state.moveMode;
    x           = arg1->panel.contentLeft.signedValue + 0x78;
    span        = arg1->panel.contentRight.signedValue - x;
    columnCount = 2;
    do {
        if (i != selected) {
            look = Ui_LookupTable(arg1, 2);
        } else {
            look = Ui_LookupTable(arg1, 1);
        }
        one = 1;
        Text_DrawPrompt(arg1, x + y / columnCount, arg0->field_1A, *p, look, one, 0);
        p++;
        y += span;
        i += one;
    } while (i < 2);
    if (arg0->field_C == one) {
        if (Pad_CheckButtons(0, one, 0x2000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected += one;
            if (selected >= columnCount) {
                selected = 0;
            }
        } else if (Pad_CheckButtons(0, 1, 0x8000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            selected -= 1;
            if (selected < 0) {
                selected += columnCount;
            }
        }
    }
    Mc_SaveData[0].state.moveMode = selected;
    status                        = arg1->panel.control.word;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam(D_options_801D5D68, 0, 0);
    }
}

static void func_options_801D4B64(Task* task)
{
    UiList*   list;
    UiObject* obj;
    UiObject* child;
    s32       status;
    s32       result;

    list = &D_options_801D5EB0;
    obj  = task->spawnArg2.pointer;
    if (task->spawnArg1.value == 1) {
        list = &D_options_801D5ED8;
    }
    if (task->state == 0) {
        Ui_LayoutListPanel(list, &(obj)->panel);
        task->state += 1;
        if (task->spawnArg1.value == 1) {
            Ui_UpdateLayoutSize(&(obj)->panel, 0xC0, 0);
            obj->panel.bounds.unsignedRect.y = -((s16)obj->panel.bounds.unsignedRect.h / 2);
            obj->panel.bounds.unsignedRect.x = -((s16)obj->panel.bounds.unsignedRect.w / 2);
        }
    }
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, "Option");
    Ui_UpdateListNoAnim(list, obj);
    status = obj->panel.control.word;
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
        child  = task->firstChild->spawnArg2.pointer;
        result = child->field_2E;
        switch (result) {
            case 6:
                Ui_TeardownTree(child, child->owner);
                obj->panel.control.word = USER_INTERFACE_PANEL_ACTIVE;
                break;
            case -1:
                obj->field_2E = result;
                break;
        }
    }
}

static void func_options_801D4D0C(Task* task)
{
    UiObject*   obj       = (UiObject*)task->spawnArg2.pointer;
    u8*         labels[3] = { D_options_801D5C64, D_options_801D5C6C, D_options_801D5C74 };
    u8*         runWalk;
    TextDrawReq req0;
    TextDrawReq req1;
    TextDrawReq req2;
    TextDrawReq req3;
    TextDrawReq req4;
    TextDrawReq req5;
    TextDrawReq req6;
    TextDrawReq req7;
    TextDrawReq req8;
    TextDrawReq req9;
    TextDrawReq req10;
    KeyIconReq  last;
    s32         x;
    s32         y;
    s32         yHdr;
    s32         y1;
    s32         edge;
    s32         base;
    s32         xRight;
    s32         status;
    s32         status2;
    s32         i;
    s32         h;
    s32         w;
    s32         l1;
    s32         r1;
    s32         l2;
    s32         r2;
    s32         type;
    s32         one;
    s32         one2;
    s32         one3;
    s32         color;
    s32         color2;
    s32         two;
    s32         center;
    s32         walkMode;
    s32         barY;
    u8*         str;
    SPRT*       p;

    runWalk  = D_options_801D5C10;
    x        = obj->panel.contentTop.signedValue;
    one      = 1;
    walkMode = Mc_SaveData[0].state.moveMode;
    type     = Mc_SaveData[0].state.buttonLayout;
    y        = x + 0xF;
    if (walkMode == one) {
        runWalk = D_options_801D5C14;
    }
    obj->field_2E = 0;
    Ui_DrawText(&(obj)->panel, "Key Configuration");
    if (task->state == 0) {
        Ui_UpdateLayoutSize(&(obj)->panel, 0, Ui_Scale15(9) + 6);
        task->spawnArg1.value = Mc_SaveData[0].state.buttonLayout;
        task->state          += 1;
    }
    y1     = x + 1;
    edge   = obj->panel.contentRight.signedValue;
    xRight = edge - 0x3B;
    l1     = (s16)obj->panel.bounds.unsignedRect.x + (s16)obj->panel.bounds.unsignedRect.w;
    r1     = obj->panel.contentOriginX.signedValue + 5;
    Ui_DrawFlatCaret(&(obj)->panel, l1 - r1, y1, 0x606060, 0);
    l2 = (s16)obj->panel.bounds.unsignedRect.x + (s16)obj->panel.bounds.unsignedRect.w;
    r2 = obj->panel.contentOriginX.signedValue + 5;
    Ui_DrawFlatCaret(&(obj)->panel, l2 - r2, y, 0x606060, one);
    Text_DrawPrompt(obj, xRight, y, labels[type], 0x606060, one, 0);
    status = obj->panel.control.word;
    if (((status >> 16) == one) || (status == one)) {
        Ui_SmoothCursor(&(obj)->panel, xRight, x + 7);
        if (obj->panel.control.word == one) {
            obj->panel.otIndex.signedValue = obj->panel.otIndex.signedValue + 1;
            Ui_AllocTile(&(obj)->panel, edge - 0x40, y1, 0x3A, 0xE, 0x1741FU);
            obj->panel.otIndex.signedValue = obj->panel.otIndex.signedValue - 1;
        }
    }
    barY = y + 2;
    Ui_DrawHBar(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, barY);
    color = 0x606060;
    Ui_DrawVBar(&(obj)->panel, y + 5, obj->panel.contentBottom.signedValue, obj->panel.contentLeft.signedValue + 0x5F);
    y   += 0x13;
    base = obj->panel.contentLeft.signedValue;
    one2 = 1;
    x    = base + 0x1E;
    Text_DrawPrompt(obj, x, y, D_options_801D5C4C, color, one2, 0);
    yHdr = y;
    y   += 0xB;

    /* Left column: button names. */
    req0.x          = obj->panel.contentOriginX.unsignedValue + x;
    req0.y          = obj->panel.contentOriginY.unsignedValue + y;
    y              += 0xF;
    req0.otIndex    = obj->panel.otIndex.signedValue + one2;
    req0.field_8    = color;
    req0.glyphTable = 0;
    req0.centerMode = 0;
    req0.field_E    = one2;
    Text_DrawString(&req0, D_options_801D5BD4);

    req1.x          = obj->panel.contentOriginX.unsignedValue + x;
    req1.y          = obj->panel.contentOriginY.unsignedValue + y;
    y              += 0xF;
    req1.otIndex    = obj->panel.otIndex.signedValue + one2;
    req1.field_8    = color;
    req1.glyphTable = 0;
    req1.centerMode = 0;
    req1.field_E    = one2;
    Text_DrawString(&req1, D_options_801D5BD4);

    req2.x          = obj->panel.contentOriginX.unsignedValue + x;
    req2.y          = obj->panel.contentOriginY.unsignedValue + y;
    y              += 0xF;
    req2.otIndex    = obj->panel.otIndex.signedValue + one2;
    req2.field_8    = color;
    req2.glyphTable = 0;
    req2.centerMode = 0;
    req2.field_E    = one2;
    Text_DrawString(&req2, D_options_801D5BD0);

    req3.x          = obj->panel.contentOriginX.unsignedValue + x;
    req3.y          = obj->panel.contentOriginY.unsignedValue + y;
    y              += 0xF;
    req3.otIndex    = obj->panel.otIndex.signedValue + one2;
    req3.field_8    = color;
    req3.glyphTable = 0;
    req3.centerMode = 0;
    req3.field_E    = one2;
    Text_DrawString(&req3, D_options_801D5BDC);

    req4.x          = obj->panel.contentOriginX.unsignedValue + x;
    req4.y          = obj->panel.contentOriginY.unsignedValue + y;
    y              += 0x1E;
    req4.otIndex    = obj->panel.otIndex.signedValue + one2;
    req4.field_8    = color;
    req4.glyphTable = 0;
    req4.centerMode = 0;
    req4.field_E    = one2;
    Text_DrawString(&req4, D_options_801D5BE4);

    req5.x          = obj->panel.contentOriginX.unsignedValue + x;
    req5.y          = obj->panel.contentOriginY.unsignedValue + y;
    x               = base + 0x6A;
    y               = yHdr;
    req5.otIndex    = obj->panel.otIndex.signedValue + one2;
    req5.field_8    = color;
    req5.glyphTable = 0;
    req5.centerMode = 0;
    req5.field_E    = one2;
    Text_DrawString(&req5, D_options_801D5BF0);

    /* Middle column: run / walk assignments. */
    Text_DrawPrompt(obj, x, y, D_options_801D5C54, color, one2, 0);
    y += 0xB;
    if (type != one2) {
        req6.x          = obj->panel.contentOriginX.unsignedValue + x;
        req6.y          = obj->panel.contentOriginY.unsignedValue + y;
        req6.otIndex    = obj->panel.otIndex.signedValue + one2;
        req6.field_8    = color;
        req6.glyphTable = 0;
        req6.centerMode = 0;
        req6.field_E    = one2;
        Text_DrawString(&req6, D_options_801D5BFC);
    } else {
        req6.x          = obj->panel.contentOriginX.unsignedValue + x;
        req6.y          = obj->panel.contentOriginY.unsignedValue + y;
        req6.otIndex    = obj->panel.otIndex.signedValue + one2;
        req6.field_8    = color;
        req6.glyphTable = 0;
        req6.centerMode = 0;
        req6.field_E    = one2;
        Text_DrawString(&req6, runWalk);
    }

    y += 0xF;
    if (type != 1) {
        req6.x          = obj->panel.contentOriginX.unsignedValue + x;
        req6.y          = obj->panel.contentOriginY.unsignedValue + y;
        req6.otIndex    = obj->panel.otIndex.signedValue + 1;
        req6.field_8    = 0x606060;
        req6.glyphTable = 0;
        req6.centerMode = 0;
        req6.field_E    = 1;
        Text_DrawString(&req6, runWalk);
    } else {
        req6.x          = obj->panel.contentOriginX.unsignedValue + x;
        req6.y          = obj->panel.contentOriginY.unsignedValue + y;
        req6.otIndex    = obj->panel.otIndex.signedValue + 1;
        req6.field_8    = 0x606060;
        req6.glyphTable = 0;
        req6.centerMode = 0;
        req6.field_E    = 1;
        Text_DrawString(&req6, D_options_801D5BFC);
    }

    y              += 0xF;
    color2          = 0x606060;
    req6.x          = obj->panel.contentOriginX.unsignedValue + ((obj->panel.contentRight.signedValue + 0x60 + obj->panel.contentLeft.signedValue) / 2);
    req6.y          = obj->panel.contentOriginY.unsignedValue + y;
    y              += 0xF;
    req6.otIndex    = obj->panel.otIndex.signedValue + 1;
    req6.field_8    = color2;
    req6.glyphTable = 0;
    req6.centerMode = 1;
    req6.field_E    = 1;
    Text_DrawString(&req6, D_options_801D5C08);

    two = 2;
    if (type == two) {
        req6.x          = obj->panel.contentOriginX.unsignedValue + x;
        req6.y          = obj->panel.contentOriginY.unsignedValue + y;
        req6.otIndex    = obj->panel.otIndex.signedValue + 1;
        req6.field_8    = color2;
        req6.glyphTable = 0;
        req6.centerMode = 0;
        req6.field_E    = 1;
        Text_DrawString(&req6, D_options_801D5BFC);

        y              += 0xF;
        req7.x          = obj->panel.contentOriginX.unsignedValue + x;
        req7.y          = obj->panel.contentOriginY.unsignedValue + y;
        req7.otIndex    = obj->panel.otIndex.signedValue + 1;
        req7.field_8    = color2;
        req7.glyphTable = 0;
        req7.centerMode = 0;
        req7.field_E    = 1;
        Text_DrawString(&req7, D_options_801D5BFC);
    }

    /* Right column: per-scheme labels, right-aligned. */
    y    = yHdr;
    x    = obj->panel.contentRight.signedValue - 4;
    one3 = 1;
    Text_DrawPrompt(obj, x, y, D_options_801D5C5C, color2, one3, two);
    y     += 0xB;
    center = 2;
    if (type == 0) {
        str = D_options_801D5C1C;
    } else if (type == one3) {
        str = D_options_801D5BD4;
    } else {
        str = D_options_801D5C34;
    }
    req8.x          = obj->panel.contentOriginX.unsignedValue + x;
    req8.y          = obj->panel.contentOriginY.unsignedValue + y;
    y              += 0xF;
    req8.otIndex    = obj->panel.otIndex.signedValue + 1;
    req8.field_8    = 0x606060;
    req8.glyphTable = 0;
    req8.centerMode = center;
    req8.field_E    = 1;
    Text_DrawString(&req8, str);

    if (type == 0) {
        str = D_options_801D5BD4;
    } else if (type == 1) {
        str = D_options_801D5C1C;
    } else {
        str = D_options_801D5BD4;
    }
    req9.x          = obj->panel.contentOriginX.unsignedValue + x;
    req9.y          = obj->panel.contentOriginY.unsignedValue + y;
    y              += 0x1E;
    req9.otIndex    = obj->panel.otIndex.signedValue + 1;
    req9.field_8    = 0x606060;
    req9.glyphTable = 0;
    req9.centerMode = center;
    req9.field_E    = 1;
    Text_DrawString(&req9, str);

    if (type == 2) {
        req10.x          = obj->panel.contentOriginX.unsignedValue + x;
        req10.y          = obj->panel.contentOriginY.unsignedValue + y;
        req10.otIndex    = obj->panel.otIndex.signedValue + 1;
        req10.field_8    = 0x606060;
        req10.glyphTable = 0;
        req10.centerMode = center;
        req10.field_E    = 1;
        Text_DrawString(&req10, D_options_801D5C40);
    } else {
        req10.x          = obj->panel.contentOriginX.unsignedValue + ((obj->panel.contentRight.signedValue + 0x60 + obj->panel.contentLeft.signedValue) / 2);
        req10.y          = obj->panel.contentOriginY.unsignedValue + y;
        req10.otIndex    = obj->panel.otIndex.signedValue + 1;
        req10.field_8    = 0x606060;
        req10.glyphTable = 0;
        req10.centerMode = 1;
        req10.field_E    = 1;
        Text_DrawString(&req10, D_options_801D5C2C);
    }

    y += 0xF;
    if (type == 0) {
        str = D_options_801D5C34;
    } else if (type == 1) {
        str = D_options_801D5C34;
    } else {
        str = D_options_801D5C1C;
    }
    req10.x          = obj->panel.contentOriginX.unsignedValue + x;
    req10.y          = obj->panel.contentOriginY.unsignedValue + y;
    y               += 0xF;
    req10.otIndex    = obj->panel.otIndex.signedValue + 1;
    req10.field_8    = 0x606060;
    req10.glyphTable = 0;
    req10.centerMode = center;
    req10.field_E    = 1;
    Text_DrawString(&req10, str);

    if (type != 2) {
        last.req.x          = obj->panel.contentOriginX.unsignedValue + x;
        last.req.y          = obj->panel.contentOriginY.unsignedValue + y;
        last.req.otIndex    = obj->panel.otIndex.signedValue + 1;
        last.req.field_8    = 0x606060;
        last.req.glyphTable = 0;
        last.req.centerMode = center;
        last.req.field_E    = 1;
        Text_DrawString(&last.req, D_options_801D5C40);
    } else {
        last.req.x          = obj->panel.contentOriginX.unsignedValue + ((obj->panel.contentRight.signedValue + 0x60 + obj->panel.contentLeft.signedValue) / 2);
        last.req.y          = obj->panel.contentOriginY.unsignedValue + y;
        last.req.otIndex    = obj->panel.otIndex.signedValue + 1;
        last.req.field_8    = 0x606060;
        last.req.glyphTable = 0;
        last.req.centerMode = 1;
        last.req.field_E    = 1;
        Text_DrawString(&last.req, D_options_801D5C2C);
    }

    /* Key icons down the left edge: four 15x15 buttons, then three 15x8. */
    y = yHdr;
    x = obj->panel.contentLeft.signedValue + 2;
    i = 0;
    do {
        last.uvs       = Options_KeyIconUvs;
        w              = 0xF;
        h              = 0xF;
        p              = gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        p->y0          = y - 0xF;
        p->x0          = x;
        if (i >= 4) {
            h     = 8;
            p->y0 = y - 0xB;
        }
        y      += 0xF;
        p->w    = w;
        p->h    = h;
        p->u0   = last.uvs.pairs[i][0];
        p->v0   = last.uvs.pairs[i][1];
        p->clut = 0x3C00;
        setlen(p, 4);
        setcode(p, 0x65);
        addPrim(&gGpuCurrentOt[obj->panel.otIndex.signedValue + 1], p);
        i++;
    } while (i < 7);

    status2 = obj->panel.control.word;
    if (((status2 >> 16) == 1) || (status2 == 1)) {
        switch (type) {
            case 0:
                Ui_SetHolderParam(D_options_801D5E0C, 0, 0);
                break;
            case 1:
                Ui_SetHolderParam(D_options_801D5E20, 0, 0);
                break;
            case 2:
                Ui_SetHolderParam(D_options_801D5E58, 0, 0);
                break;
            default:
                Ui_SetHolderParam(D_options_801D5E90, 0, 0);
                break;
        }
    }
    Ui_InsertDrawTPage(obj->panel.otIndex.signedValue + 1, 0);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (Pad_CheckButtons(0, 1, 0x6000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            Mc_SaveData[0].state.buttonLayout = ((s8)(Mc_SaveData[0].state.buttonLayout + 1)) % 3;
        } else if (Pad_CheckButtons(0, 1, 0x9000) != 0) {
            SndEvt_EnqueueType6(2, 0, 0);
            Mc_SaveData[0].state.buttonLayout = ((s8)(Mc_SaveData[0].state.buttonLayout + 2)) % 3;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }
}

/* Defined after the function so its rodata follows the function's own
   constants (the label table and title string), as in the original layout. */
static const KeyIconUvs Options_KeyIconUvs = { {
    { 0x10, 0x60 },
    { 0x10, 0x70 },
    { 0x20, 0x60 },
    { 0x20, 0x70 },
    { 0x90, 0x58 },
    { 0xB0, 0x58 },
    { 0x80, 0x58 },
    { 0xA0, 0x58 },
} };

static void func_options_801D5954(UiList* arg0, UiObject* arg1)
{
    s32 status;

    Text_DrawPrompt(arg1, arg1->panel.contentLeft.signedValue + 6, arg0->field_1A, D_options_801D5BAC, arg0->field_1C, 1, 0);
    status = arg1->panel.control.word;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam(D_options_801D5DA4, 0, 0);
    }
    if ((arg0->field_C == 1) && (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0)) {
        SndEvt_EnqueueType6(3, 0, 0);
        Ui_SpawnFromDesc(&D_options_801D5EFC, 0, 1, 1, arg1);
        arg1->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
    }
}

static void func_options_801D5A4C(UiList* arg0, UiObject* arg1)
{
    s32 status;

    Text_DrawPrompt(arg1, arg1->panel.contentLeft.signedValue + 6, arg0->field_1A, D_options_801D5B2C, arg0->field_1C, 1, 0);
    status = arg1->panel.control.word;
    if ((((status >> 0x10) == 1) || (status == 1)) && (arg0->field_10 == arg0->field_8)) {
        Ui_SetHolderParam(D_options_801D5DDC, 0, 0);
    }
    if ((arg0->field_C == 1) && (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0)) {
        SndEvt_EnqueueType6(3, 0, 0);
        Mc_ResetSaveFlags();
    }
}
