#include "common.h"
#include "main/task.h"

extern TaskDesc RoomsShared8017e320Desc;
extern Task*    RoomsShared8017e320Task;
extern Task*    D_dryfield_night_gas_station_801907AC;

/// Retires the room's third tracked task and drops the room's reference to it.
/// The `-1` state is the task's own exit request, so the task frees itself on
/// its next tick.
void func_dryfield_night_gas_station_80180974(void)
{
    if (RoomsShared8017e320Task != NULL) {
        RoomsShared8017e320Task->state = -1;
        RoomsShared8017e320Task        = NULL;
    }
}

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

/// Second teardown entry point for `RoomsShared8017e320Task`: requests the
/// task's exit and drops the room's reference to it, exactly as
/// `func_dryfield_night_gas_station_80180974` does.
void func_dryfield_night_gas_station_80180B38(void)
{
    if (RoomsShared8017e320Task != NULL) {
        RoomsShared8017e320Task->state = -1;
        RoomsShared8017e320Task        = NULL;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_5", func_dryfield_night_gas_station_80180B5C);
