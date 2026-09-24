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

#include "rooms/room_common.h"
#include "rooms/neo_ark_bridge.h"

extern s32 Gp_LcgState;

/// `Gp_State1C` effect task drawing a growing, fading quad through
/// `func_neo_ark_bridge_8017F0C4`. The first frame sets the brightness to
/// 0x40, takes the size from the spawn parameter's low 12 bits and turns the
/// coordinate to a random Y rotation. Every frame then rebuilds the
/// coordinate, grows the size by 0x20, draws, and dims by 2, releasing the
/// effect once the brightness falls under 2. Once the room's event state
/// leaves zero it only draws, and releases at state 4.
void func_neo_ark_bridge_8017EF70(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_neo_ark_bridge_8017F0C4(coord, (s16)work->field_26, (s16)work->field_24);
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
        func_neo_ark_bridge_8017F0C4(coord, (s16)work->field_26, (s16)work->field_24);
        work->field_24 -= 2;
        if ((s16)work->field_24 < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}
