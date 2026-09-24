#include "common.h"
#include "main/task.h"

#include "rooms/dryfield_night_gas_station.h"

/// Spawns entry 0 of the room's task table and tracks it in
/// `D_dryfield_night_gas_station_801907A4`.
void func_dryfield_night_gas_station_801807A0(void)
{
    D_dryfield_night_gas_station_801907A4 = Task_SpawnFromTable(&D_dryfield_night_gas_station_801888A0, 0, 0, 0);
}
