#include "common.h"

#include "gameplay/gameplay.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void func_dryfield_night_warehouse_8017D6B4(GsCOORDINATE2* coord, s32 arg1);
void func_dryfield_night_warehouse_8017DFF4(GsCOORDINATE2* coord, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_warehouse/dryfield_night_warehouse_2", func_dryfield_night_warehouse_8017D6B4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_warehouse/dryfield_night_warehouse_2", func_dryfield_night_warehouse_8017DFF4);

/// Per-frame effect on the room's model task: recomputes the model's world
/// matrix and then re-poses it. The current visit is the stage-visit byte
/// `gGameSession->loc.view` taken as a bit index, and each pose is gated on that
/// bit being one of a fixed set of visits.
void func_dryfield_night_warehouse_8017E778(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    coord = ((TmdObject*)arg0->extra)->field_8;
    mask  = 1 << gGameSession->loc.view;
    Gp_UpdateCoord(coord);
    if (mask & 0x24C) {
        func_dryfield_night_warehouse_8017D6B4(coord, 8);
    }
    if (mask & 4) {
        func_dryfield_night_warehouse_8017DFF4(coord, 0, 8);
    }
    if (mask & 0x24C) {
        func_dryfield_night_warehouse_8017DFF4(coord, 2, 8);
    }
    if (mask & 0x3DC) {
        func_dryfield_night_warehouse_8017DFF4(coord, 4, 8);
        func_dryfield_night_warehouse_8017DFF4(coord, 6, 8);
    }
}
