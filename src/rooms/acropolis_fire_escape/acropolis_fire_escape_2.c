#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/pad.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"

extern UiObject*    D_80067634;
extern UiObjectDesc D_800611E4;
extern UiList       RoomsShared8017ea68List;
void                RoomsShared80180c98(UiList* list, UiObject* obj);
void                RoomsShared80180f94(UiList* list, UiObject* obj);

extern UiList       D_acropolis_fire_escape_80181C6C;
extern UiObjectDesc D_acropolis_fire_escape_80181C90;

void func_acropolis_fire_escape_8017ED60(Task* task)
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
