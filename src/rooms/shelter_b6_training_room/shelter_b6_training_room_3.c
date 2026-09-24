#include "common.h"
#include "main/task.h"

#include "rooms/shelter_b6_training_room.h"

extern TaskDesc D_shelter_b6_training_room_801839A8;

void func_shelter_b6_training_room_8017D940(void)
{
    D_shelter_b6_training_room_80185C5C = Task_SpawnFromTable(&D_shelter_b6_training_room_801839A8, 0, 0, 0);
}
