#include "common.h"

#include "main/task.h"
#include "rooms/mist_parking.h"

/// Spawns entry 3 of `D_mist_parking_8018D75C`.
void func_mist_parking_8018354C(void)
{
    Task_SpawnFromTable(&D_mist_parking_8018D75C, 3, 0, 0);
}
