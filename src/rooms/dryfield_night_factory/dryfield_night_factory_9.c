#include "common.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"

extern s8 D_8007216C;

void func_dryfield_night_factory_8018182C(Task* task)
{
    NightFactoryScriptWork* work;
    RoomHotspot*            hs;

    work = Mem_Calloc(0x10, 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(D_dryfield_night_factory_80186E94, 0, 1, 0);
    task->work      = (TaskIdMap*)work;
    task->field_24  = D_dryfield_night_factory_80186EAC;
    if (GameFlag_GetNibble(0x48) == 0) {
        D_8007216C = 0xC;
    } else {
        D_8007216C = 5;
    }
    task->state++;
    Display_AcquireRef();
    for (hs = D_dryfield_night_factory_80186EBC; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    Game_Session->field_66 = 1;
    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
    work->field_8          = 0;
}
