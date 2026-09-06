#include "common.h"

#include "main/task.h"
#include "rooms/rooms_shared_8017ea2c.h"

extern TaskDesc RoomsShared8017e5b8Desc;

void RoomsShared8017ea2c(void)
{
    RoomsShared8017ea2cTask = Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 4, 0, 0);
}
