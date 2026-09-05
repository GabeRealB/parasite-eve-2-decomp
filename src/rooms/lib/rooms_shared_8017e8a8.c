#include "common.h"

#include "main/task.h"
#include "rooms/rooms_shared_8017e8a8.h"

void RoomsShared8017e8a8(s32 arg0)
{
    if (RoomsShared8017e8a8Task != NULL) {
        RoomsShared8017e8a8Task->state = arg0;
    }
}
