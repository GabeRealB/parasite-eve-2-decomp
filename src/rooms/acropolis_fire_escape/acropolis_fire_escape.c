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

extern UiList       RoomsShared8017e8b4List;
extern UiObjectDesc RoomsShared8017e8b4Desc;

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", D_acropolis_fire_escape_8017D610);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017de9cHundred);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017e8b4WeaponTitle);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017e8b4PeTitle);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", RoomsShared8017ea68Title);
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
