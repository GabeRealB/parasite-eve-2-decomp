#include "common.h"
#include "main/task.h"

#include "rooms/rooms_shared_801807d4.h"

extern TaskDesc RoomsShared8017e320Desc;

/// Spawns the room's first tracked task (slot 0 of the shared table) and stores
/// it in `RoomsShared801807d4Task`.
void func_dryfield_night_gas_station_801807A0(void)
{
    RoomsShared801807d4Task = Task_SpawnFromTable(&RoomsShared8017e320Desc, 0, 0, 0);
}
