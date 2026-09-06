#include "common.h"
#include "gameplay/3688.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/ui.h"
#include "rooms/room_common.h"
#include <psyq/libgte.h>

extern UiList       D_shelter_b6_nursery_80184F30;
extern UiObjectDesc D_shelter_b6_nursery_80184F54;

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
