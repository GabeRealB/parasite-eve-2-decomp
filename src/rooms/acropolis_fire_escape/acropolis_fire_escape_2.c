#include "common.h"

#include <psyq/libgte.h>

#include "main/pad.h"
#include "main/task.h"
#include "main/ui.h"

extern char         D_acropolis_fire_escape_8017D610[];
extern UiList       D_acropolis_fire_escape_80181C44;
extern UiObjectDesc RoomsShared8017e8b4Desc;

void func_acropolis_fire_escape_8017EF20(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_acropolis_fire_escape_80181C44;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_acropolis_fire_escape_8017D610);
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