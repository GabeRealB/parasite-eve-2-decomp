#include "common.h"

#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ed38.h"
#include "rooms/rooms_shared_8017f280.h"

void RoomsShared8017f280(Task* task)
{
    TaskFunc states[2] = { Room_Util04, RoomsShared8017ed38 };

    states[task->state](task);
}
