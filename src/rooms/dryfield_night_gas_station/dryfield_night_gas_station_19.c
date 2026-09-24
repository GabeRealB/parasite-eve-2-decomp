#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_gas_station.h"

/// Spawns entry 1 of the room's task table and tracks it in
/// `D_dryfield_night_gas_station_801907A8`.
void func_dryfield_night_gas_station_80180940(void)
{
    D_dryfield_night_gas_station_801907A8 = Task_SpawnFromTable(&D_dryfield_night_gas_station_801888A0, 1, 0, 0);
}
