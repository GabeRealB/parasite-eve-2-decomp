#include "common.h"

#include <psyq/libgte.h>

#include "main/mem.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e8b4.h"

void RoomsShared80180c98(UiList* list, UiObject* obj);
void RoomsShared80180f94(UiList* list, UiObject* obj);

void RoomsShared8017e8b4(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &RoomsShared8017e8b4List;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, RoomsShared8017e8b4WeaponTitle);
    } else {
        Ui_DrawText((UiPanel*)obj, RoomsShared8017e8b4PeTitle);
    }
    if (task->state == 0) {
        work = Mem_Calloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->idMap = work;
        Ui_SpawnFromDesc(&RoomsShared8017e8b4Desc, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            RoomsShared80180c98(list, obj);
        } else {
            RoomsShared80180f94(list, obj);
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
