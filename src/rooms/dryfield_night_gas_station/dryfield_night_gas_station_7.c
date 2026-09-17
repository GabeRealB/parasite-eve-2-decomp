#include "common.h"
#include "main/task.h"

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/dryfield_night_gas_station.h"
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

/// Switches the room's lamp effect between its lit and dark appearance: the
/// current room's three lamp views have their three flags written to 1 for the
/// 0 argument and to 0 for the 1 argument, and any other argument changes
/// nothing. `func_dryfield_night_gas_station_80180A60` drives it from the
/// blinking-light table, whose own exit passes 0.
void func_dryfield_night_gas_station_80180DC8(s16 arg0)
{
    GameSessionFrom4*               sess = (GameSessionFrom4*)&Game_Session->field_4;
    DryfieldNightGasStationSprtRec* rec  = (DryfieldNightGasStationSprtRec*)
                                              Gp_SprtTables[sess->field_3 - 1][0]
                                                  .field_0[sess->field_2 - 1];
    DryfieldNightGasStationSprtView* view;

    switch (arg0) {
        case 0:
            view           = rec->field_A0;
            view->field_44 = 1;
            view->field_4C = 1;
            view           = rec->field_AC;
            view->field_44 = 1;
            view->field_4C = 1;
            view->field_54 = 1;
            view           = rec->field_C4;
            view->field_44 = 1;
            view->field_4C = 1;
            view->field_54 = 1;
            break;
        case 1:
            view           = rec->field_A0;
            view->field_44 = 0;
            view->field_4C = 0;
            view           = rec->field_AC;
            view->field_44 = 0;
            view->field_4C = 0;
            view->field_54 = 0;
            view           = rec->field_C4;
            view->field_44 = 0;
            view->field_4C = 0;
            view->field_54 = 0;
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station_7", func_dryfield_night_gas_station_80180E9C);
