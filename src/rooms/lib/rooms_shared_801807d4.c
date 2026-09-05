#include "common.h"

#include "main/task.h"
#include "rooms/rooms_shared_801807d4.h"

void RoomsShared801807d4(s32 arg0)
{
    Task* t = RoomsShared801807d4Task;

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
    Task_Kill(RoomsShared801807d4Task);
    RoomsShared801807d4Task = NULL;
}
