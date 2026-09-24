#include "common.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"

extern s8 D_8007216C;

extern TaskDesc    D_dryfield_factory_80186E88[];
extern GpMsgEntry  D_dryfield_factory_80186EA0[];
extern RoomHotspot D_dryfield_factory_80186EB0[];

/// State 0 of the room's script task: allocates its work block, spawns the
/// child task, publishes the script's message table, picks the global mode
/// byte from game flag 0x48, advances, and clears the hotspot hits while
/// holding the HUD for the cutscene.
void func_dryfield_factory_8018182C(Task* task)
{
    NightFactoryScriptWork* work;
    RoomHotspot*            hs;

    work = memCalloc(0x10, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(D_dryfield_factory_80186E88, 0, 1, 0);
    task->work      = (TaskIdMap*)work;
    task->msgTable  = D_dryfield_factory_80186EA0;
    if (GameFlag_GetNibble(0x48) == 0) {
        D_8007216C = 0xC;
    } else {
        D_8007216C = 5;
    }
    task->state++;
    Display_AcquireRef();
    for (hs = D_dryfield_factory_80186EB0; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    work->field_8              = 0;
}
