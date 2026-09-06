#include "common.h"

#include "main/task.h"

#include "rooms/rooms_shared_8018459c.h"
#include "rooms/rooms_shared_801845d0.h"

void RoomsShared801845d0(s32 arg0)
{
    Task* t = RoomsShared8018459cTask;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    Task_Kill(RoomsShared8018459cTask);
    RoomsShared8018459cTask = NULL;
}
