#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_water_hole.h"
#include "rooms/room_common.h"

extern s32 Gp_LcgState;

/// Per-frame driver of an expanding, fading flash effect. While the room's
/// event state is 0 it updates the task's coordinate, ticks the age counter
/// `field_22` and draws the flash through
/// `func_dryfield_water_hole_8017EDE4` at size `field_26` and brightness
/// `field_24`. The first frame sets the brightness to 0x40, takes the size from
/// the spawn argument's low 12 bits and turns the coordinate about Y by a
/// random angle; every frame then grows the size by 0x20 and dims the
/// brightness by 2, releasing the work block once it falls under 2. Once the
/// event state is non-zero it only draws, releasing the block from event state
/// 4 on.
void func_dryfield_water_hole_8017EC90(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_dryfield_water_hole_8017EDE4(coord, (s16)work->field_26, (s16)work->field_24);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->field_22++;
        if (task->state == 0) {
            work->field_24 = 0x40;
            work->field_26 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->flg  = 0;
            task->state = 1;
        }
        work->field_26 += 0x20;
        func_dryfield_water_hole_8017EDE4(coord, (s16)work->field_26, (s16)work->field_24);
        work->field_24 -= 2;
        if ((s16)work->field_24 < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}
