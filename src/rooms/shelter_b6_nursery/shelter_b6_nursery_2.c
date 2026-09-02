#include "common.h"
#include <psyq/libgte.h>
#include "main/pad.h"
#include "main/task.h"
#include "main/ui.h"
extern char         D_shelter_b6_nursery_8017D610[];
extern UiList       D_shelter_b6_nursery_80184F08;
extern UiObjectDesc D_shelter_b6_nursery_80184F54;

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_2", func_shelter_b6_nursery_8017EE88);

void func_shelter_b6_nursery_8017EF7C(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_shelter_b6_nursery_80184F08;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_shelter_b6_nursery_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_shelter_b6_nursery_80184F54, 0, 0, 1, obj);
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
