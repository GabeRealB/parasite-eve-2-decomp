#include "common.h"

#include "gameplay/3CD8.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_gas_station.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_gas_station_80183144;

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_5", func_dryfield_gas_station_80181058);

/// Per-frame effect: draws the gas station's shaft with the task's own model
/// coordinate, then advances the room's `Gp_State1C`. `Task::extra` is the
/// task's `TmdObject`, so `field_8` is the coordinate both draws share. The
/// stage-visit byte `gGameSession->loc.view` is used as a bit index: bits 4, 6,
/// 11 and 12 (`0x1850`) select `Room_Draw37` with the wide half-extent 0x80,
/// and any other non-zero bit selects `func_dryfield_gas_station_80181058`
/// with 0x40.
void func_dryfield_gas_station_80181A78(Task* arg0)
{
    s32            mask;
    GsCOORDINATE2* coord;

    mask  = 1 << gGameSession->loc.view;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (mask & 0x1850) {
        Room_Draw37(coord, &D_dryfield_gas_station_80183144, 0x60, 0x80);
    } else if (mask != 0) {
        func_dryfield_gas_station_80181058(coord, &D_dryfield_gas_station_80183144, 0x60, 0x40);
    }
    Gp_State1C->field_A = 2;
}
