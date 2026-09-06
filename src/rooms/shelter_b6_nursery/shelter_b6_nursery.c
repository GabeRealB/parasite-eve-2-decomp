#include "common.h"
#include "gameplay/3688.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/ui.h"
#include "rooms/room_common.h"
#include <psyq/libgte.h>

extern UiList       RoomsShared8017e8b4List;
extern UiObjectDesc RoomsShared8017e8b4Desc;

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", D_shelter_b6_nursery_8017D610);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", RoomsShared8017de9cHundred);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", RoomsShared8017e8b4WeaponTitle);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", RoomsShared8017e8b4PeTitle);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery", RoomsShared8017ea68Title);
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
