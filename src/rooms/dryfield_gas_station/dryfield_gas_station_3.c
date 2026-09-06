#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"

#include "rooms/dryfield_gas_station.h"

extern void     Stage_RequestFromAreaTable(s32 arg0);
extern TaskDesc D_dryfield_gas_station_80181E7C[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_3", func_dryfield_gas_station_8017FD54);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_3", func_dryfield_gas_station_8017FE20);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_3", func_dryfield_gas_station_8017FEDC);

void func_dryfield_gas_station_8017FF84(void)
{
}
