#include "common.h"
#include "main/task.h"

extern TaskDesc RoomsShared8017e320Desc;
extern Task*    RoomsShared8017e320Task;
extern Task*    D_dryfield_night_gas_station_801907AC;

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_5", func_dryfield_night_gas_station_80180974);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_5", func_dryfield_night_gas_station_80180998);

/// Spawns the room's second tracked task (slot 2 of the shared table) and
/// stores it beside `RoomsShared8017e320Task`.
void func_dryfield_night_gas_station_80180A00(void)
{
    D_dryfield_night_gas_station_801907AC = Task_SpawnFromTable(&RoomsShared8017e320Desc, 2, 0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_5", func_dryfield_night_gas_station_80180A34);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_5", func_dryfield_night_gas_station_80180A60);

/// Spawns the room's third tracked task (slot 3 of the shared table) and stores
/// it in `RoomsShared8017e320Task`, the slot the room's teardown clears.
void func_dryfield_night_gas_station_80180B04(void)
{
    RoomsShared8017e320Task = Task_SpawnFromTable(&RoomsShared8017e320Desc, 3, 0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_5", func_dryfield_night_gas_station_80180B38);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_5", func_dryfield_night_gas_station_80180B5C);
