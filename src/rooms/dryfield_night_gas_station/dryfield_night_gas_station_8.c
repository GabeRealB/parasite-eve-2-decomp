#include "common.h"
#include "main/task.h"
extern s32   D_dryfield_night_gas_station_801907A4;
extern Task* RoomsShared8017e320Task;
extern s32   D_dryfield_night_gas_station_801907AC;

void func_dryfield_night_gas_station_80180C20(void)
{
    D_dryfield_night_gas_station_801907A4 = 0;
    RoomsShared8017e320Task               = 0;
    D_dryfield_night_gas_station_801907AC = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_8", func_dryfield_night_gas_station_80180C3C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_8", func_dryfield_night_gas_station_80180D1C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_8", func_dryfield_night_gas_station_80180DC8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_8", func_dryfield_night_gas_station_80180E9C);
