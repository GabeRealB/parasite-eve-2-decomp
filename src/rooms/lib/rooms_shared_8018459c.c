#include "common.h"

#include "main/task.h"
#include "rooms/rooms_shared_8018459c.h"

extern TaskDesc RoomsShared8017daf0Desc;

void RoomsShared8018459c(void)
{
    RoomsShared8018459cTask = Task_SpawnFromTable(&RoomsShared8017daf0Desc, 1, 0, 0);
}
