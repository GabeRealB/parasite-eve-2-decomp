#include "common.h"

#include <psyq/libgte.h>

#include "main/pad.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/rooms_shared_8017e8b4.h"
#include "rooms/rooms_shared_8017ef20.h"

void RoomsShared8017ef20(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &RoomsShared8017ef20List;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, RoomsShared8017ef20Title);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&RoomsShared8017e8b4Desc, 0, 0, 1, obj);
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->field_12 += 5;
        list->field_A  = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
}
