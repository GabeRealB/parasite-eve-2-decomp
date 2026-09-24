#include "common.h"

#include "main/task.h"
#include "rooms/mist_parking.h"

/// Spawns entry 0 of `D_mist_parking_80190824`.
void func_mist_parking_801844EC(void)
{
    Task_SpawnFromTable(&D_mist_parking_80190824, 0, 0, 0);
}
