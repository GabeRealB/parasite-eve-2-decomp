#include "common.h"
#include "main/task.h"

#include "rooms/rooms_shared_801807d4.h"

extern TaskDesc D_shelter_b6_training_room_801839A8;

void func_shelter_b6_training_room_8017D940(void)
{
    RoomsShared801807d4Task = Task_SpawnFromTable(&D_shelter_b6_training_room_801839A8, 0, 0, 0);
}
