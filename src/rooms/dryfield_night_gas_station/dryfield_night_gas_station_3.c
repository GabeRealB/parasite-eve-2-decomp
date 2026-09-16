#include "common.h"

#include "main/task.h"

void func_dryfield_night_gas_station_80180D1C(void);

extern TaskFuncTable3 D_dryfield_night_gas_station_8017D644;

/// Gates the room's two sprite records on nibble 0x8D, then dispatches the task
/// through the room's own three-state table, copied onto the stack first.
void func_dryfield_night_gas_station_8017FB70(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_gas_station_8017D644;
    func_dryfield_night_gas_station_80180D1C();
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_3", func_dryfield_night_gas_station_8017FBD4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_3", func_dryfield_night_gas_station_8017FD80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_3", func_dryfield_night_gas_station_801802EC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_3", func_dryfield_night_gas_station_80180604);
