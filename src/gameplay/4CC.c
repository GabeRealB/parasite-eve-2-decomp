#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "main/gamemain.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

extern s32 D_8005ED74;
extern s32 D_8005ED78;

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BC634);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BCC44);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BCEA4);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BD2FC);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BD6DC);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BDAA8);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BDC80);

void func_800BDDC4(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;
    UiObject* child;
    s32       flag;
    Task*     parent;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    menu          = &D_8010D68C;
    if (arg0->state == 0) {
        parent = arg0->parent;
        if (parent != NULL) {
            arg0->flags = parent->flags;
        }
        if (((s16)obj->field_E + (s16)obj->field_12) >= 0x65) {
            obj->field_E = 0x64 - obj->field_12;
        }
        func_800BDC80(menu, obj);
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        arg0->state = arg0->state + 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, D_8005ED78) != 0) {
            if (obj->owner->flags != 0) {
                obj->field_2E = 6;
            } else {
                obj->status   = 0;
                obj->field_2E = -1;
            }
        } else if (Pad_CheckButtons(0, 1, D_8005ED74) != 0) {
            obj->field_2E = 6;
        }
    }
    child = (UiObject*)arg0->firstChild;
    if (child != NULL) {
        child = ((Task*)child)->spawnArg2;
        flag  = child->field_2E;
        switch (flag) {
            case -1:
                obj->status   = 0;
                obj->field_2E = flag;
                break;
            case 9:
                obj->field_2E = 6;
                break;
            case 6:
                obj->status = 1;
                Ui_TeardownTree(child, child->owner);
                break;
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BDF6C);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BE808);

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BEBE4);

void func_800BF2C8(UiObject* arg0, void (*arg1)(UiObject*, Task*))
{
    Task* owner;
    Task* child;
    Task* next;
    Task* head;

    owner = arg0->owner;
    child = owner->firstChild;
    if (child != NULL) {
        do {
            next = child->nextSibling;
            arg1(child->spawnArg2, child);
            head  = owner->firstChild;
            child = next;
            if (head == NULL) {
                break;
            }
        } while (child != head);
    }
}

s32 func_800BF334(s32 arg0, s32 arg1)
{
    s32 ret;

    ret = 0;
    if (D_8010D838[arg0].field_3 & 1) {
        ret = arg1 == 1;
    }
    if ((D_80114D7C == 0x703) && (arg0 == 0x81) && (Mc_SaveData.field_7 == 1)) {
        ret = 1;
    }
    return ret;
}

void func_800BF398(UiObject* arg0, Task* arg1)
{
    UiObject* parent;

    parent = arg1->parent->spawnArg2;
    switch (arg0->field_2E) {
        case -1:
            if (parent->owner->flags) {
                parent->status = 1;
                Ui_TeardownTree(arg0, arg0->owner);
            } else {
                Ui_TeardownTree(arg0, arg0->owner);
                parent->status   = 0;
                parent->field_2E = -1;
            }
            break;
        case 6:
            parent->status = 1;
            Ui_TeardownTree(arg0, arg0->owner);
            break;
        case 0x23:
            Ui_TeardownTree(arg0, arg0->owner);
            parent->field_2E = 0x23;
            break;
        case 38:
        case 39:
            parent->field_2E = arg0->field_2E;
            break;
    }
}

void func_800BF464(Task* arg0)
{
    UiObject* obj;
    UiList*   menu;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    menu          = &D_8010D6B4;
    if (arg0->state == 0) {
        Ui_LayoutListPanel(menu, (UiPanel*)obj);
        if (arg0->spawnArg1 == 0) {
            menu->field_10 = 1;
        } else {
            menu->field_10 = 0;
        }
        menu->field_A = 1;
        arg0->state  += 1;
    }
    Ui_UpdateListNoAnim(menu, obj);
}

void func_800BF4FC(Task* arg0)
{
    UiObject* obj;
    s32       val;
    s32       color;
    s32       one;
    u8*       text;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        Wip_UiHolder = (WipUiHolder*)obj;
        arg0->state += 1;
    }
    val = arg0->spawnArg1;
    if (val != 0) {
        color = 0x606060;
        one   = 1;
        Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, (u8*)val, color, one, 0);
        text = Text_SkipLines((u8*)val, one);
        Text_DrawPrompt(obj, obj->field_1C + 2, (s16)obj->field_18 + 0x1E, text, color, one, 0);
    }
}

s32 func_800BF5CC(Task* arg0, s32 arg1, GpItemObj2* arg2)
{
    s32         flag;
    GpItemObj8* obj;

    obj              = arg0->spawnArg2;
    flag             = 1;
    arg0->flags      = flag;
    arg0->extraState = (s32)arg2;
    if (func_800BB470(obj->field_8) == 2) {
        arg2->field_2 = flag;
    }
    return 0;
}

void func_800BF624(Task* arg0)
{
    u16 item;
    s32 count;

    {
        register GpItemObj8* obj asm("v1");
        register u16         ritem asm("a0");

        obj        = arg0->spawnArg2;
        ritem      = obj->field_8;
        D_80114DEC = ritem;
        ritem      = obj->field_A;
        D_80114DDC = ritem;
        item       = ritem;
    }
    if (item < 0xA0) {
        if ((u16)(item - 0x60) < 0x20U) {
            if (func_800B7420(D_80114DDC) != 0) {
                D_80114DDC = 0xD;
            }
        } else if ((u16)(item - 0x80) < 0x20U) {
            if (func_800B7420(D_80114DDC) != 0) {
                D_80114DDC = 0x3D;
            }
        }
        D_80114DD0 = 1;
        D_80114DC8 = 1;
    } else {
        count      = D_8010E3B8[D_80114DDC - 0xA0].field_0;
        D_80114DC8 = 1;
        D_80114DD0 = count;
    }
    GameMain_SetFrameTiming(0);
    Wip_UiHolder        = NULL;
    arg0->killCountdown = 1;
    arg0->state         = arg0->state + 1;
}

INCLUDE_ASM("gameplay/nonmatchings/4CC", func_800BF738);
