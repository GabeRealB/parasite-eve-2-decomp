#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

#include <psyq/libgs.h>

extern s32 Gp_LcgState;

/// Per-frame driver for one of a room's `Gp_State1C` effect tasks, carried by
/// eleven rooms. While the room is still fading in (`Gp_State1C::field_4 == 0`)
/// it advances the task's own coordinate frame, ticks the lifetime counter and
/// walks the size ramp (`field_24`, dropped by 2 a frame), drawing the effect at
/// the ramp's current size each frame; on the first frame it seeds the size from
/// the LCG and the angle from the spawn parameter, and once the ramp falls under
/// 2 it releases the effect. After the fade (`field_4 >= 4`) it only draws and
/// releases, so the effect lingers through the rest of the fade without
/// advancing.
void RoomsShared8017f4a0(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        Room_Draw16(coord, (s16)work->field_26, (s16)work->field_24);
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
        Room_Draw16(coord, (s16)work->field_26, (s16)work->field_24);
        work->field_24 -= 2;
        if ((s16)work->field_24 < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}
