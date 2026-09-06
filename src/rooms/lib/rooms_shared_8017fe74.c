#include "common.h"

#include <psyq/libgte.h>

#include "main/pad.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/ui.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017fe74.h"

void RoomsShared8017fe74(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s16       code;

    list          = &RoomsShared8017fe74List;
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
