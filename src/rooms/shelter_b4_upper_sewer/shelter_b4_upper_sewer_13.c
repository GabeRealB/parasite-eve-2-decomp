#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"
#include "rooms/shelter_b4_upper_sewer.h"

#include <psyq/libgs.h>

extern s32 Gp_LcgState;

/// Effect task drawing a flat quad through `func_shelter_b4_upper_sewer_8017EA0C`
/// that grows and fades out. On its first frame it starts the size (`field_26`)
/// at the low 12 bits of the spawn argument and the brightness (`field_24`) at
/// 0x40, and turns its coordinate to a random angle about Y. Each frame it then
/// grows the size by 0x20 and dims the brightness by 2, releasing the effect
/// once the brightness falls under 2. While `Gp_State1C->eventState` is
/// non-zero it only redraws at the current values, and releases from state 4.
void func_shelter_b4_upper_sewer_8017E8B8(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_upper_sewer_8017EA0C(coord, (s16)work->field_26, (s16)work->field_24);
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
        func_shelter_b4_upper_sewer_8017EA0C(coord, (s16)work->field_26, (s16)work->field_24);
        work->field_24 -= 2;
        if ((s16)work->field_24 < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}
