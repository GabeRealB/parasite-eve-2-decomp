#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/ui.h"
#include "main/wipsys.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ed7c.h"

extern UiObject*    D_80067634;
extern UiObjectDesc D_8010D80C;

void RoomsShared8017ed7c(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    Task*     head;
    UiObject* childObj;
    s32       code;

    obj           = task->spawnArg2;
    list          = &RoomsShared8017ed7cList;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, (char*)RoomsShared8017ed7cTitle);
    if (task->state == 0) {
        Gp_ClearPreviewItems();
        D_80067634 = NULL;
        Ui_SpawnFromDesc(&RoomsShared8017ed7cRowsDesc, task->spawnArg1, 0, 1, obj);
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
