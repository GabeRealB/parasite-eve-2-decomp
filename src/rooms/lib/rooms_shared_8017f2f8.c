#include "common.h"

#include "main/task.h"

#include "rooms/rooms_shared_8017d5f0.h"
#include "rooms/rooms_shared_8017f2f8.h"

void RoomsShared8017f2f8(Task* task)
{
    TaskFunc states[2] = {
        RoomsShared8017d5f0,
        RoomsShared8017d7a4,
    };

    states[task->state](task);
}
