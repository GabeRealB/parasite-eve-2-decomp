#include "common.h"

#include "gameplay/3688.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/ui.h"

#include <psyq/libgte.h>

extern void Room_SaveUi01(Task* task);

extern UiList       D_shelter_b6_nursery_80184F30;
extern UiObjectDesc D_shelter_b6_nursery_80184F54;

void func_shelter_b6_nursery_8017E2F4(UiList* list, UiObject* obj);
void func_shelter_b6_nursery_8017E5F0(UiList* list, UiObject* obj);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", D_shelter_b6_nursery_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", func_shelter_b6_nursery_8017D72C);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", D_shelter_b6_nursery_8017D610);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", func_shelter_b6_nursery_8017DEF8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", func_shelter_b6_nursery_8017E2F4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", func_shelter_b6_nursery_8017E5F0);

void func_shelter_b6_nursery_8017E910(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_shelter_b6_nursery_80184F30;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, "Weapon Data");
    } else {
        Ui_DrawText((UiPanel*)obj, "PE Data");
    }
    if (task->state == 0) {
        work = Mem_Calloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->idMap = work;
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F54, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_shelter_b6_nursery_8017E2F4(list, obj);
        } else {
            func_shelter_b6_nursery_8017E5F0(list, obj);
        }
        Ui_InitList(list, (UiMiniObj*)obj);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2;
            next     = child->nextSibling;
            if (childObj->field_2E == -1 || childObj->field_2E == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->status = 1;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", func_shelter_b6_nursery_8017EAC4);

void func_shelter_b6_nursery_8017EDBC(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = Room_SaveUi01;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}
