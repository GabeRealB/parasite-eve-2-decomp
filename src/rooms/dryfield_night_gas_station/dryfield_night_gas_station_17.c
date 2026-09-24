#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_gas_station.h"

/// Passes `arg0` to the task tracked in `D_dryfield_night_gas_station_801907A4`
/// as its `spawnArg1` when it is 0 or 1; any other value kills the task and
/// clears the handle.
void func_dryfield_night_gas_station_801807D4(s32 arg0)
{
    Task* t = D_dryfield_night_gas_station_801907A4;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    taskKill(D_dryfield_night_gas_station_801907A4);
    D_dryfield_night_gas_station_801907A4 = NULL;
}
