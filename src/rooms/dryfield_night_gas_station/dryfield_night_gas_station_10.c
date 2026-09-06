#include "common.h"
#include "main/task.h"

#include "rooms/rooms_shared_801807d4.h"

extern Task* RoomsShared8017e320Task;
extern s32   D_dryfield_night_gas_station_801907AC;

void func_dryfield_night_gas_station_80180C20(void)
{
    RoomsShared801807d4Task               = 0;
    RoomsShared8017e320Task               = 0;
    D_dryfield_night_gas_station_801907AC = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_10", func_dryfield_night_gas_station_80180C3C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_10", func_dryfield_night_gas_station_80180D1C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_10", func_dryfield_night_gas_station_80180DC8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_10", func_dryfield_night_gas_station_80180E9C);
