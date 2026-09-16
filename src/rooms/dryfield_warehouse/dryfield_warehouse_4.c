#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void func_dryfield_warehouse_8017E414(GsCOORDINATE2* coord, s32 arg1);
void func_dryfield_warehouse_8017ED34(GsCOORDINATE2* coord, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse_4", func_dryfield_warehouse_8017E414);

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse_4", func_dryfield_warehouse_8017ED34);

/// Per-frame effect on the room's model task: re-poses the model for the
/// current stage visit, then publishes variant 2 as the room's
/// `Gp_State1C::field_A` index. `Task::extra` is the task's `TmdObject`, so
/// `field_8` is the coordinate every pose shares. The stage-visit byte
/// `Game_Session->field_4` is used as a bit index: bits 2, 3, 6 and 9 (`0x24C`)
/// pose through `func_dryfield_warehouse_8017E414`, bit 2 (`4`) also drives
/// `func_dryfield_warehouse_8017ED34` to step 0, those same `0x24C` visits also
/// drive it to step 2, and bits 2, 3, 4 and 6-9 (`0x3DC`) drive it to steps 4
/// and 6.
void func_dryfield_warehouse_8017F494(Task* arg0)
{
    s32            mask;
    s32            poseMask;
    GsCOORDINATE2* coord;

    mask     = 1 << Game_Session->field_4;
    poseMask = mask & 0x24C;
    coord    = ((TmdObject*)arg0->extra)->field_8;
    if (poseMask != 0) {
        func_dryfield_warehouse_8017E414(coord, 8);
    }
    if (mask & 4) {
        func_dryfield_warehouse_8017ED34(coord, 0, 8);
    }
    if (poseMask != 0) {
        func_dryfield_warehouse_8017ED34(coord, 2, 8);
    }
    if (mask & 0x3DC) {
        func_dryfield_warehouse_8017ED34(coord, 4, 8);
        func_dryfield_warehouse_8017ED34(coord, 6, 8);
    }
    Gp_State1C->field_A = 2;
}
