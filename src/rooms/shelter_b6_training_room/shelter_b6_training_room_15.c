#include "common.h"

#include "main/task.h"

extern TaskDesc D_shelter_b6_training_room_8018431C;

/// Spawns the task that starts the room's stream playback.
void func_shelter_b6_training_room_8017DAC8(void)
{
    Task_SpawnFromTable(&D_shelter_b6_training_room_8018431C, 0, 0, 0);
}
