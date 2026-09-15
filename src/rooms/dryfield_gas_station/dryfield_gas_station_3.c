#include "common.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"
#include "rooms/dryfield_gas_station.h"

extern void     Stage_RequestFromAreaTable(s32 arg0);
extern u8       D_80072170;
extern u8       D_80115598;
extern TaskDesc D_dryfield_gas_station_80181E3C[];
extern s32      D_dryfield_gas_station_80181E54;
extern TaskDesc D_dryfield_gas_station_80181E7C[];
extern Task*    D_dryfield_gas_station_80184BCC;

INCLUDE_RODATA("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_3", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_3", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_3", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_3", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_3", RoomsShared8017ea68Title);

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_3", func_dryfield_gas_station_8017FD54);

/// Spawns the room's event task and stores it in `D_dryfield_gas_station_80184BCC`,
/// waits for it to be killed, then kills this task.
void func_dryfield_gas_station_8017FE20(Task* arg0)
{
    s32 state = arg0->state;
    s32 out;

    switch (state) {
        case 0:
            D_dryfield_gas_station_80184BCC = Task_SpawnFromTable(D_dryfield_gas_station_80181E7C, 0, 0, 0);
            arg0->state++;
            break;
        case 1:
            if (Task_PollKill(D_dryfield_gas_station_80184BCC, &out) != 0) {
                arg0->state++;
            }
            break;
        case 2:
            Task_Kill(arg0);
            break;
    }
}

/// State 0 of the gas-station cutscene task. On the first visit
/// (`D_80072170 == 1`) it spawns the room's event task and clears the three
/// progression flags; otherwise it just asks the stage for area 1. Either way
/// it advances to state 1 and raises the `D_80115598` flag.
void func_dryfield_gas_station_8017FEDC(Task* arg0)
{
    arg0->field_24 = &D_dryfield_gas_station_80181E54;
    Game_SetPtrSlot(arg0, 7);
    if (D_80072170 == 1) {
        Task_SpawnFromTable(D_dryfield_gas_station_80181E3C, 0, 0, 0);
        GameFlag_SetNibble(0x7A, 2);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 0);
    } else {
        Stage_RequestFromAreaTable(1);
    }
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

void func_dryfield_gas_station_8017FF84(void)
{
}
