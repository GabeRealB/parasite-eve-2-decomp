#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_gas_station.h"

/// Hands the cutscene task the script command `arg0` to carry out, starting
/// it from its first step.
void func_dryfield_gas_station_80180B2C(s16 arg0)
{
    DgsWork* work = (DgsWork*)D_dryfield_gas_station_80184BD4->work;

    work->field_4 = arg0;
    work->field_6 = 0;
}
