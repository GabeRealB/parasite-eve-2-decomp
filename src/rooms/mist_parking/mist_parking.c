#include "common.h"
#include <psyq/libgte.h>
#include "main/display.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/4CC.h"

extern UiObject*    D_80067634;
extern u8           D_mist_parking_8017D6D8[];
extern u8           D_mist_parking_80186464[];
extern UiList       D_mist_parking_80186540;
extern UiObjectDesc D_mist_parking_801865C8;
extern UiList       D_mist_parking_8018656C;
extern UiObjectDesc D_mist_parking_80186590;

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017DF68);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017E3F4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017E540);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017E90C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017EB5C);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D6D8);

void func_mist_parking_8017ED7C(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2;
    list          = &D_mist_parking_80186540;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, "List");
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&D_mist_parking_801865C8, task->spawnArg1, 0, 1, obj);
        Ui_SpawnFromDesc(&D_8010D80C, 0, 0, 0, obj);
        list->field_4 = 5;
        list->field_5 = 5;
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0) {
        obj->field_2E = -1;
    }

    head = task->firstChild;
    if (head != NULL) {
        child = head;
        do {
            childObj = child->spawnArg2;
            code     = childObj->field_2E;
            next     = child->nextSibling;
            if (code != -1) {
                if (code == 6) {
                    Ui_TeardownTree(childObj, childObj->owner);
                    obj->status = 1;
                }
            } else {
                Wip_UiHolder  = NULL;
                obj->field_2E = code;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

void func_mist_parking_8017EF24(Task* task)
{
    s8            digits[0x20];
    s8            total[0x20];
    TextDrawReq   req0;
    TextDrawReq   req1;
    UiObject*     obj;
    WipSysConfig* cfg;
    McItemScan*   scan;
    s8*           p;
    s32           x;
    s32           y;
    s32           y2;
    s32           col;
    s32           capacity;
    s32           count;

    obj = task->spawnArg2;
    cfg = &Wip_SysConfig;
    x   = (s16)obj->field_1C + 2;
    col = (s16)obj->field_1E - 2;
    y   = (s16)obj->field_18;

    req0.x          = obj->baseX + x;
    req0.y          = obj->baseY + y + 9;
    req0.otIndex    = (s16)obj->drawOrder + 1;
    req0.field_8    = 0x606060;
    req0.glyphTable = 5;
    req0.centerMode = 0;
    req0.field_E    = 1;
    func_8002E53C(&req0, D_mist_parking_8017D6D8);

    Text_ItoaUnsigned((u8*)digits, cfg->field_C);
    Text_DrawPrompt(obj, col, y + 0x19, (u8*)digits, 0x606060, 3, 2);

    y2              = y + 0x28;
    req1.x          = obj->baseX + x;
    req1.y          = obj->baseY + (y2 - 6);
    req1.otIndex    = (s16)obj->drawOrder + 1;
    req1.field_8    = 0x606060;
    req1.glyphTable = 5;
    req1.centerMode = 0;
    req1.field_E    = 1;
    func_8002E53C(&req1, "TOTAL");

    p        = total;
    scan     = &Mc_SaveData.field_5BC;
    count    = Gp_CountScanItems(scan);
    capacity = scan->field_1;
    Text_ItoaUnsigned((u8*)p, count);
    while (*p != 0) {
        p++;
    }
    *p = '/';
    Text_ItoaUnsigned((u8*)(p + 1), capacity);
    Text_DrawPrompt(obj, col, y2 + 0xA, (u8*)total, 0x606060, 3, 2);
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F108);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F31C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F49C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F764);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8017F938);

void func_mist_parking_8017FDB8(DialogPrompt* prompt, UiObject* obj)
{
    TextDrawReq req;

    req.x          = obj->baseX + (u16)prompt->field_18;
    req.y          = obj->baseY + (u16)prompt->field_1A;
    req.otIndex    = (s16)obj->drawOrder + 1;
    req.field_8    = prompt->field_1C;
    req.glyphTable = 0;
    req.centerMode = 0;
    req.field_E    = 1;
    func_8002E53C(&req, D_mist_parking_80186464);

    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        obj->field_2E = 6;
    }
}

void func_mist_parking_8017FE74(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &D_mist_parking_8018656C;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
            SndEvt_EnqueueType6(4, 0, 0);
            obj->field_2E = 6;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskMenu) != 0) {
            obj->field_2E = -1;
        }
    }

    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        code     = childObj->field_2E;
        if (code != -1) {
            if (code == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->status = 1;
            }
        } else {
            obj->field_2E = -1;
        }
    }
}

void func_mist_parking_8017FF9C(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Display_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_mist_parking_80186590, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        Game_Session->field_2 = 1;
        task->spawnArg2       = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            Ui_TeardownTree(obj, obj->owner);
            task->killCountdown = 10;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            Game_Session->field_2 = 0;
            Task_Kill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_801800D0);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking", D_mist_parking_8017D748);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_8018089C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_80180C98);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_80180F94);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_801812B4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_80181468);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking", func_mist_parking_80181760);
