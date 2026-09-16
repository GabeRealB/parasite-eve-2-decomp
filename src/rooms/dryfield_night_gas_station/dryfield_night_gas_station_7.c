#include "common.h"
#include "main/task.h"

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/rooms_shared_801807d4.h"

extern Task* RoomsShared8017e320Task;
extern Task* D_dryfield_night_gas_station_801907AC;

void func_dryfield_night_gas_station_80180C20(void)
{
    RoomsShared801807d4Task               = 0;
    RoomsShared8017e320Task               = 0;
    D_dryfield_night_gas_station_801907AC = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_7", func_dryfield_night_gas_station_80180C3C);

/// Gates the room's two sprite command records on game flag nibble 0x8D: a
/// zero nibble clears both commands' skip-link flag, a one sets it. The two
/// records are views 10 and 19 of the current room's sprite record array, and
/// the flag both write is command 6's.
void func_dryfield_night_gas_station_80180D1C(void)
{
    GameSessionFrom4* sess = (GameSessionFrom4*)&Game_Session->field_4;
    GpSprtRec*        view = Gp_SprtTables[sess->field_3 - 1][0].field_0[sess->field_2 - 1];
    s32               flag = GameFlag_GetNibble(0x8D);

    switch (flag) {
        case 0:
            view[10].field_4[6].field_4 = 0;
            view[19].field_4[6].field_4 = 0;
            break;
        case 1:
            view[10].field_4[6].field_4 = flag;
            view[19].field_4[6].field_4 = flag;
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_7", func_dryfield_night_gas_station_80180DC8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_7", func_dryfield_night_gas_station_80180E9C);
