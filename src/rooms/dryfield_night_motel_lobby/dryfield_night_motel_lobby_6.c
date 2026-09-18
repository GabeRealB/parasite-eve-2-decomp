#include "common.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_night_motel_lobby.h"
#include "rooms/room_common.h"

extern s8       D_8007216C;
extern TaskDesc D_dryfield_night_motel_lobby_80182814[];

void func_dryfield_night_motel_lobby_80180E98(Task* task)
{
    DnmlExamineWork* work;
    RoomHotspot*     hs;
    u8*              p;
    u8               empty;
    s32              i;

    work = Mem_Calloc(0xA, 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->spawnArg2 = Task_SpawnFromTable(D_dryfield_night_motel_lobby_80182814, 0, 1, 0);
    task->work      = (TaskIdMap*)work;
    D_8007216C      = 6;
    /* The once-loop folds away, but `flow` counts its references at loop depth
       2: without it the state load is scheduled above the mode store. */
    do {
        task->state++;
    } while (0);
    Display_AcquireRef();
    for (hs = D_dryfield_night_motel_lobby_80182820; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    /* The fill value has to reach the store through a register and the pointer
       has to be built from the index: a literal store, or a `&code[6]` folded
       into the symbol, compiles to a different loop. */
    empty = 0xA;
    i     = 6;
    p     = &D_dryfield_night_motel_lobby_801844D8[i];
    for (; i >= 0; i--) {
        *p-- = empty;
    }
    gGameSession->field_66 = 1;
    gGameSession->field_68 = 1;
    gGameSession->field_1  = 1;
}
