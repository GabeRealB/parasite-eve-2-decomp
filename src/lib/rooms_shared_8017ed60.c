#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "main/task.h"
#include "main/ui.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ed60.h"

void RoomsShared8017ed60(Task* task)
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
