#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_gas_station.h"

/// The three states of the task run by `func_dryfield_gas_station_8017FF8C`.
extern const TaskFuncTable3 D_dryfield_gas_station_8017D6A4;

/// Dispatches the task through the room's three-state table, copied onto the
/// stack first.
void func_dryfield_gas_station_8017FF8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_gas_station_8017D6A4;
    sp.funcs[task->state](task);
}
