#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern u32 Gp_LcgState;

/// Per-frame driver of a rising sprite effect task.
///
/// On its first frame it seeds the rise speed (`field_10.vy`, 0x10-0x4F from
/// the LCG, negated when bit 16 of `Task::spawnArg1` is set), a random spin
/// angle (`field_24`) and the sprite radius (`field_26`, the low 12 bits of
/// `Task::spawnArg1`). While no event runs, every frame moves the coordinate
/// frame along Y by the speed and advances the animation frame (`field_20`)
/// every fourth tick, releasing the effect after frame 7. During an event of
/// state 1-3 it keeps drawing without moving or animating; state 4 or above
/// releases it. Each draw picks one of six sprite CLUTs at random.
void func_shelter_b2_pod_bottom_80180898(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
            return;
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_DrawFxQuad(coord, work->field_20, work->field_26, work->field_24 | (((Gp_LcgState >> 16) % 6) << 12));
        return;
    }
    work->field_22++;
    if (task->state == 0) {
        work->field_10.vx = 0;
        work->field_10.vz = 0;
        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
        work->field_10.vy = ((Gp_LcgState >> 16) & 0x3F) + 0x10;
        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
        work->field_24    = (Gp_LcgState >> 16) & 0xFFF;
        work->field_26    = task->spawnArg1 & 0xFFF;
        if (task->spawnArg1 & 0x10000) {
            work->field_10.vy = -work->field_10.vy;
        }
        task->state = 1;
    }
    coord->coord.t[1] += work->field_10.vy;
    coord->flg         = 0;
    if (!(work->field_22 & 3)) {
        work->field_20++;
    }
    if ((s16)work->field_20 < 8) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_DrawFxQuad(coord, work->field_20, work->field_26, work->field_24 | (((Gp_LcgState >> 16) % 6) << 12));
        return;
    }
    Gp_ReleaseState1CMem(work, task);
}
